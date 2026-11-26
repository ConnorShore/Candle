#include "cdlpch.h"
#include "Renderer.h"

#include "Candle/Render/Vulkan/RenderDevice.h"
#include "Candle/Render/Pipelines/PipelineSpecification.h"
#include "Candle/Render/FramePacket.h"
#include "Candle/Render/FrameContext.h"
#include "Candle/Render/Vulkan/SwapChainImage.h"
#include "Candle/Render/Resources/GeometryPool.h"
#include "Candle/Render/Resources/TexturePool.h"
#include "Candle/Render/Vulkan/GPUAllocator.h"
#include "Candle/Render/RenderConstants.h"
#include "Candle/Render/GUI/GuiDrawData.h"
#include "Candle/Render/GUI/GuiVertex.h"

#include "Candle/Render/FrameGraph/FrameGraph.h"
#include "Candle/Render/FrameGraph/FrameGraphHandles.h"

#include "Candle/Render/FrameGraph/Modules/SceneModule.h"
#include "Candle/Render/FrameGraph/Modules/DebugModule.h"
#include "Candle/Render/FrameGraph/Modules/GuiModule.h"
#include "Candle/Render/FrameGraph/Modules/FrameGraphPreviewModule.h"
#include "Candle/Render/FrameGraph/Modules/GlobalModuleData.h"

namespace Candle {

	namespace {

		vk::Format FindDepthFormat(RenderDevice& device)
		{
			// Depth-only: nothing uses stencil, and a view with both aspects can't be sampled. The spec requires D16 and one of
			// the first two as attachments, so the panic below can't be reached on a conformant device.
			const std::vector<vk::Format> preferredFormats = {
				vk::Format::eD32Sfloat,
				vk::Format::eX8D24UnormPack32,
				vk::Format::eD16Unorm
			};
			for (const auto& format : preferredFormats)
			{
				vk::FormatProperties formatProperties = device.GetPhysicalDevice().getFormatProperties(format);
				if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
					return format;
			}
			CDL_CORE_PANIC(LogChannel::Render, "No suitable depth format found");
		}
	}

	Renderer::Renderer(RenderDevice& renderDevice, GeometryPool& geometryPool, TexturePool& texturePool, BindlessTable& bindlessTable,
		GPUAllocator& allocator, const RendererCreationInfo& info)
		: m_RenderDevice(renderDevice)
		, m_GeometryPool(geometryPool)
		, m_TexturePool(texturePool)
		, m_BindlessTable(bindlessTable)
		, m_PipelineStore(renderDevice, bindlessTable, info.PipelineCacheDirectory)
		, m_FrameGraph({ .Allocator = allocator, .BindlessTable = bindlessTable, .TexturePool = texturePool, .PipelineStore = m_PipelineStore })
		, m_RenderToSwapChain(info.RenderToSwapChain)
		, m_TargetFormat(info.TargetFormat)
	{
		RenderModuleSpec sceneModuleSpec{
			.Graph = m_FrameGraph,
			.Pipelines = m_PipelineStore,
			.ColorFormat = info.TargetFormat,
			.DepthFormat = FindDepthFormat(m_RenderDevice),
			.ShaderDirectory = info.ShaderDirectory
		};
		m_RenderModules.emplace_back(ScopedPtr<SceneModule>::Create(sceneModuleSpec, geometryPool, texturePool));
		m_RenderModules.emplace_back(ScopedPtr<DebugModule>::Create(sceneModuleSpec, renderDevice));
		m_RenderModules.emplace_back(ScopedPtr<FrameGraphPreviewModule>::Create(sceneModuleSpec));
		m_RenderModules.emplace_back(ScopedPtr<GuiModule>::Create(sceneModuleSpec, texturePool));

		// Save the pipeline cache to disk so that it can be loaded next time the engine is run
		m_PipelineStore.SaveCache();
	}

	void Renderer::Render(const FramePacket& packet, FrameContext& frame, SwapChainImage& target)
	{
		const glm::uvec2 sceneExtent = m_RenderToSwapChain ? target.GetDesc().Extent : m_TexturePool.GetTextureInfo(packet.OutputTarget).Desc.Extent;
		m_FrameGraph.Reset(sceneExtent);

		const FGTextureHandle swapChainHandle = m_FrameGraph.ImportSwapChainImage(target);
		const FGTextureHandle sceneColor = m_RenderToSwapChain ? swapChainHandle : m_FrameGraph.ImportTexture("OutputTarget", packet.OutputTarget);

		// Add render modules' passes to the frame graph
		Blackboard& blackboard = m_FrameGraph.GetBlackboard();

		FrameTargetData outputData{
			.SwapChain = swapChainHandle,
			.Output = sceneColor
		};
		blackboard.Set<FrameTargetData>(outputData);
		CDL_PROFILE_SCOPE("Renderer::Render");

		const GPU::GlobalPushConstants global{
			.Frame = frame.FrameData.Address
		};

		RenderFrameInputs inputs{
			.Packet = packet,
			.Frame = frame,
			.Global = global
		};
		for (const auto& module : m_RenderModules)
			module->AddPasses(inputs);

		// Compile the frame graph
		m_FrameGraph.Compile(packet.FrameIndex);
		m_FrameGraph.EvictUnusedResources(packet.FrameIndex,
			m_RenderDevice.GetLastSubmittedTimelineValue(QueueType::Graphics),
			m_RenderDevice.GetCompletedTimelineValue(QueueType::Graphics));

		// Recorded into the frame's command buffer, which RenderManager submits
		frame.CommandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		m_BindlessTable.BindSet(*frame.CommandBuffer, vk::PipelineBindPoint::eGraphics, m_PipelineStore.GetGlobalLayout());
		m_BindlessTable.BindSet(*frame.CommandBuffer, vk::PipelineBindPoint::eCompute, m_PipelineStore.GetGlobalLayout());

		m_FrameGraph.Execute(m_RenderDevice, frame.CommandBuffer);

		frame.CommandBuffer.end();

		// The final barrier left it ready to present; anything reading the tracked state before the next acquire, like a test's readback, expects that
		target.SetState(m_FrameGraph.GetTexture(swapChainHandle).FinalState);
	}

}