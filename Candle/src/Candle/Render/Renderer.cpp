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

// TODO: Probably should live in pipeline but see once render graph is made
#include "Candle/Render/GUI/GuiInterop.h"

#include "Candle/Render/Pipelines/TrianglePipeline.h"
#include "Candle/Render/Pipelines/TriangleInterop.h"

#include "Candle/Render/Pipelines/GUI/GuiPipeline.h"

#include <cmath>
#include <span>

namespace Candle {

	namespace {
		// A float time steps ~0.24 ms at an hour; wrapping keeps shader animation smooth for one jump per period
		constexpr double kShaderTimeWrapSeconds = 3600.0;

		vk::Format FindDepthFormat(RenderDevice& device)
		{
			// Depth-only, so the depth-aspect view and barrier stay valid without separateDepthStencilLayouts. The spec requires
			// D16 and one of the first two as attachments, so the panic below can't be reached on a conformant device.
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
		, m_GPUAllocator(allocator)
		, m_TargetFormat(info.TargetFormat)
		, m_DepthFormat(FindDepthFormat(renderDevice))
		, m_DepthImage({ .Extent = { info.TargetExtent.width, info.TargetExtent.height }, // Sized to the colour target
			.Format = m_DepthFormat,
			.Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
			.Aspect = vk::ImageAspectFlagBits::eDepth,
			.DebugName = "Depth Target" }, allocator)
	{
		CreatePipeline(TrianglePipeline{ info.ShaderDirectory });
		CreatePipeline(GuiPipeline{ info.ShaderDirectory });
	}

	void Renderer::Render(const FramePacket& packet, FrameContext& frame, SwapChainImage& target)
	{
		CDL_PROFILE_SCOPE("Renderer::Render");

		// TODO: The render graph will own the passes, their pipelines, the depth target and the barriers between them;
		//	until then the passes are recorded here by hand

		// Write the transform data into the frame's TransformData buffer, if any
		std::span<const glm::mat4> transforms = packet.TransformData;
		if (transforms.size() > kMaxTransforms)
		{
			// Checked in every config, since the write below would run past the end of the buffer
			CDL_CORE_ASSERT(false, "FramePacket has more transforms than kMaxTransforms");
			transforms = transforms.first(kMaxTransforms);
		}

		if (!transforms.empty())
			frame.TransformData.Write(std::as_bytes(transforms));

		// Safe to overwrite: RenderManager waited on this slot's RetireValue, and vkQueueSubmit makes host writes visible to the GPU
		const GPU::FrameData frameData{ 
			.ViewProjection = packet.ProjectionMatrix * packet.ViewMatrix,
			.TransformData = frame.TransformData.GetDeviceAddress(),
			.TransformCount = static_cast<uint32_t>(transforms.size()),
			.Time = static_cast<float>(std::fmod(packet.Time, kShaderTimeWrapSeconds))
		};
		frame.FrameData.Write(std::as_bytes(std::span(&frameData, 1)));

		const GPU::GlobalPushConstants global{ 
			.Frame = frame.FrameData.GetDeviceAddress()
		};

		// Begin recording commands into the command buffer
		frame.CommandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		// Each pass transitions the target into the state it needs, so the tracked state also orders it after the pass before
		RenderTrianglePipeline(packet, frame, target, global);	// Clears the target, so it runs every frame, even with nothing to draw
		RenderGuiPipeline(packet, frame, target, global);		// Loads the target and draws over the scene; returns early with no GUI

		// Every pass leaves the target as a colour attachment, and presentation reads it next
		target.Transition({
			.Layout = vk::ImageLayout::ePresentSrcKHR,
			.Access = vk::AccessFlagBits2::eNone
			}, frame.CommandBuffer);

		// End recording commands into the command buffer. After this, the command buffer can be submitted to the graphics queue for execution.
		frame.CommandBuffer.end();
	}

	void Renderer::RecreateTargets(vk::Extent2D targetExtent, GPUAllocator& allocator)
	{
		m_DepthImage.Recreate({ .Extent = { targetExtent.width, targetExtent.height },
			.Format = m_DepthFormat,
			.Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
			.Aspect = vk::ImageAspectFlagBits::eDepth,
			.DebugName = "Depth Target" }, allocator);
	}

	void Renderer::CreatePipeline(const RenderPipeline& pipeline)
	{
		PipelineSpecification spec = pipeline.CreateSpecification(m_RenderDevice, m_TargetFormat, m_DepthFormat);
		spec.DebugName = pipeline.GetName();
		m_Pipelines.emplace_back(spec, m_RenderDevice, m_BindlessTable);
	}

	void Renderer::RenderTrianglePipeline(const FramePacket& packet, FrameContext& frame, SwapChainImage& target, const GPU::GlobalPushConstants globalPCs)
	{
		// Write only: the clear replaces whatever was there
		target.Transition({
			.Layout = vk::ImageLayout::eColorAttachmentOptimal,
			.Access = vk::AccessFlagBits2::eColorAttachmentWrite,
			.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput
			}, frame.CommandBuffer);

		// Transition the depth image to DEPTH_ATTACHMENT_OPTIMAL
		vk::ImageMemoryBarrier2 depthBarrier = MakeImageBarrier(
			m_DepthImage.GetImage(),
			// Undefined discards the old contents, which the clear would overwrite anyway. Every frame in flight shares this one image,
			// so wait for the previous frame's depth writes (tests and store op), or this transition and clear race them
			{ .Layout = vk::ImageLayout::eUndefined,
				.Access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				.Stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests },
			// The clear writes in early tests, the depth compare reads and writes in early or late tests
			{ .Layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
				.Access = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				.Stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests },
			{ .aspectMask = vk::ImageAspectFlagBits::eDepth, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }		// The depth image is a single 2D image, so we only have one mip level and one array layer
		);
		frame.CommandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &depthBarrier });

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

		vk::RenderingAttachmentInfo colorAttachmentInfo = {
			.imageView = target.GetImageView(),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor };

		vk::RenderingAttachmentInfo depthAttachmentInfo = {
			.imageView = m_DepthImage.GetImageView(),
			.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = clearDepth
		};

		vk::Extent2D swapChainExtent = target.GetDesc().Extent;

		// Setup the rendering info for the render pass begin info. This specifies the render area, the number of layers, and the color attachment info.
		vk::RenderingInfo renderingInfo = {
			.renderArea = {
				.offset = { 0, 0 },
				.extent = swapChainExtent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo,
			.pDepthAttachment = &depthAttachmentInfo
		};

		{
			GPUDebugScope mainPass(m_RenderDevice, frame.CommandBuffer, "Main Triangle Pass");
			frame.CommandBuffer.beginRendering(renderingInfo);

			frame.CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Pipelines[0].GetPipeline());

			frame.CommandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(swapChainExtent.height), static_cast<float>(swapChainExtent.width), -static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
			frame.CommandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

			// Bind the bindless descriptor set to the pipeline layout
			vk::PipelineLayout pipelineLayout = m_Pipelines[0].GetLayout();
			m_BindlessTable.BindSet(*frame.CommandBuffer, vk::PipelineBindPoint::eGraphics, pipelineLayout);

			// Draw each item
			for (const DrawItem& item : packet.DrawItems)
			{
				// The GPU doesn't bounds-check a read through a device address, so skip a bad index in every config
				if (item.TransformIndex >= packet.TransformData.size())
				{
					CDL_CORE_ASSERT(false, "DrawItem's TransformIndex is past the packet's TransformData");
					continue;
				}

				MeshDrawInfo drawInfo = m_GeometryPool.GetMeshInfo(item.Mesh);
				TextureInfo textureInfo = m_TexturePool.GetTextureInfo(item.Texture);

				GPU::MeshDrawPushConstants pushConstants{
					.Global = globalPCs,
					.Vertices = drawInfo.VerticesAddr,
					.TextureIndex = textureInfo.Index,
					.SamplerIndex = static_cast<uint32_t>(item.Sampler),
					.TransformIndex = item.TransformIndex,
				};
				m_Pipelines[0].PushConstants(*frame.CommandBuffer, pushConstants);
				frame.CommandBuffer.bindIndexBuffer(drawInfo.Indices, 0, vk::IndexType::eUint32);
				frame.CommandBuffer.drawIndexed(drawInfo.IndexCount, 1, 0, 0, 0);
			}

			frame.CommandBuffer.endRendering();
		}
	}

	void Renderer::RenderGuiPipeline(const FramePacket& packet, FrameContext& frame, SwapChainImage& target, const GPU::GlobalPushConstants globalPCs)
	{
		const GuiDrawData& gui = packet.GuiDrawData;
		if (gui.Commands.empty())
			return;

		// Write the GUI vertex and index data into the frame's GuiVertexData and GuiIndexData buffers, if any
		EnsureCapacity(frame.GuiVertices, gui.Vertices.size() * sizeof(Candle::GPU::GuiVertex), vk::BufferUsageFlagBits::eShaderDeviceAddress, "GuiVertices");
		EnsureCapacity(frame.GuiIndices, gui.Indices.size() * sizeof(GuiIndex), vk::BufferUsageFlagBits::eIndexBuffer, "GuiIndices");
		frame.GuiVertices.Write(std::as_bytes(std::span(gui.Vertices)));
		frame.GuiIndices.Write(std::as_bytes(std::span(gui.Indices)));

		// Read as well as write: the load and blending both read what the scene pass wrote, so this orders after its writes
		target.Transition({
			.Layout = vk::ImageLayout::eColorAttachmentOptimal,
			.Access = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
			.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput
			}, frame.CommandBuffer);

		// Color attachment only, no depth. Loaded rather than cleared, so the GUI draws over the scene
		vk::RenderingAttachmentInfo colorAttachmentInfo = {
			.imageView = target.GetImageView(),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eLoad,
			.storeOp = vk::AttachmentStoreOp::eStore };

		vk::Extent2D swapChainExtent = target.GetDesc().Extent;

		// Setup the rendering info for the render pass begin info. 
		// This specifies the render area, the number of layers, and the color attachment info.
		vk::RenderingInfo renderingInfo = {
			.renderArea = {
				.offset = { 0, 0 },
				.extent = swapChainExtent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo,
			.pDepthAttachment = nullptr
		};

		{
			GPUDebugScope guiPass(m_RenderDevice, frame.CommandBuffer, "GUI Pass");

			frame.CommandBuffer.beginRendering(renderingInfo);

			frame.CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Pipelines[1].GetPipeline());

			// Not flipped: Vulkan's NDC already has Y pointing down, as ImGui's coordinates do
			frame.CommandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));

			// Maps ImGui's display rect [DisplayPos, DisplayPos + DisplaySize] to NDC [-1, 1]
			const glm::vec2 scale = 2.0f / gui.DisplaySize;
			const glm::vec2 translate = -1.0f - gui.DisplayPos * scale;

			// Bind the bindless descriptor set to the pipeline layout
			vk::PipelineLayout pipelineLayout = m_Pipelines[1].GetLayout();
			m_BindlessTable.BindSet(*frame.CommandBuffer, vk::PipelineBindPoint::eGraphics, pipelineLayout);

			// Bind the GUI vertex and index buffers
			frame.CommandBuffer.bindIndexBuffer(frame.GuiIndices.GetBuffer(), 0, vk::IndexType::eUint16);
			const vk::DeviceAddress vertexBase = frame.GuiVertices.GetDeviceAddress();

			// Draw each item
			uint32_t samplerIndex = static_cast<uint32_t>(SamplerType::LinearClamp);	// The GUI pipeline only uses one sampler
			for (const GuiDrawCommand& cmd : gui.Commands)
			{
				// The clip rect is already in framebuffer pixels; clamp it, since a scissor can't start below zero
				const glm::vec2 clipMin = glm::max(glm::vec2(cmd.ClipRect.x, cmd.ClipRect.y), glm::vec2(0.0f));
				const glm::vec2 clipMax = glm::min(glm::vec2(cmd.ClipRect.z, cmd.ClipRect.w), glm::vec2(swapChainExtent.width, swapChainExtent.height));
				if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
					continue;	// Entirely clipped

				frame.CommandBuffer.setScissor(0, vk::Rect2D(
					vk::Offset2D(static_cast<int32_t>(clipMin.x), static_cast<int32_t>(clipMin.y)),
					vk::Extent2D(static_cast<uint32_t>(clipMax.x - clipMin.x), static_cast<uint32_t>(clipMax.y - clipMin.y))));

				GPU::GuiDrawPushConstants pushConstants{
						.Global = globalPCs,
						.Vertices = vertexBase + cmd.VertexOffset * sizeof(Candle::GPU::GuiVertex),  // Offset here rather than as drawIndexed's vertexOffset, so the shader indexes from 0
						.Scale = scale,
						.Translate = translate,
						.TextureIndex = m_TexturePool.GetTextureInfo(cmd.Texture).Index,
						.SamplerIndex = samplerIndex
				};
				m_Pipelines[1].PushConstants(*frame.CommandBuffer, pushConstants);
				frame.CommandBuffer.drawIndexed(cmd.IndexCount, 1, cmd.IndexOffset, 0, 0);
			}

			frame.CommandBuffer.endRendering();
		}
	}

	void Renderer::EnsureCapacity(GPUBuffer& buffer, size_t size, vk::BufferUsageFlags usage, std::string_view name)
	{
		if (buffer.GetSize() >= size)
			return;

		buffer = m_GPUAllocator.CreateBuffer({
				.Size = std::max<size_t>(size, buffer.GetSize() * 2),   // Doubling, so a growing UI doesn't reallocate every frame
				.Usage = usage,
				.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				.DebugName = std::string(name) });
	}
}