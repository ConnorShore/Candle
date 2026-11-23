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
#include "Candle/Render/Pipelines/TriangleInterop.h"
#include "Candle/Render/Pipelines/DebugDrawInterop.h"

#include "Candle/Render/Pipelines/ShaderLoader.h"

#include "Candle/Render/FrameGraph/FrameGraph.h"
#include "Candle/Render/FrameGraph/FrameGraphHandles.h"

#include <cmath>
#include <span>

namespace Candle {

	namespace {
		// A float time steps ~0.24 ms at an hour; wrapping keeps shader animation smooth for one jump per period
		constexpr double kShaderTimeWrapSeconds = 3600.0;

		constexpr vk::ImageSubresourceRange kColorRange{ .aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1 };

		// A color attachment the next pass only writes, since its clear replaces whatever was there
		constexpr ImageState kColorAttachmentWrite{
			.Layout = vk::ImageLayout::eColorAttachmentOptimal,
			.Access = vk::AccessFlagBits2::eColorAttachmentWrite,
			.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput };

		// A color attachment the next pass also reads: its load and blending read what the pass before it wrote
		constexpr ImageState kColorAttachmentReadWrite{
			.Layout = vk::ImageLayout::eColorAttachmentOptimal,
			.Access = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
			.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput };

		// Storage images are only ever written in General
		constexpr ImageState kComputeStorageWrite{
			.Layout = vk::ImageLayout::eGeneral,
			.Access = vk::AccessFlagBits2::eShaderStorageWrite,
			.Stage = vk::PipelineStageFlagBits2::eComputeShader };

		// A texture the next pass samples in its fragment shader, in the layout its bindless descriptor was written with
		ImageState FragmentSampled(const TextureInfo& texture)
		{
			return { .Layout = texture.SampledLayout, .Access = vk::AccessFlagBits2::eShaderSampledRead, .Stage = vk::PipelineStageFlagBits2::eFragmentShader };
		}

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

		// The device reports its range whether or not wideLines is enabled, but without it only 1 is valid
		glm::vec2 FindLineWidthRange(RenderDevice& device)
		{
			if (!device.HasWideLines())
				return { 1.0f, 1.0f };

			const auto& range = device.GetPhysicalDevice().getProperties().limits.lineWidthRange;
			return { range[0], range[1] };
		}

		GPUImageSpec DepthImageSpec(vk::Format format, glm::uvec2 extent)
		{
			return { .Extent = extent,
				.Format = format,
				.Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
				.Aspect = vk::ImageAspectFlagBits::eDepth,
				.DebugName = "Depth Target" };
		}

		// The renderer can't draw anything without its own shaders, so a missing or bad .spv is fatal
		// TODO: Temporary until the asset manager loads shaders
		SharedPtr<Shader> LoadEngineShader(const std::string& name, const std::filesystem::path& filePath)
		{
			auto shader = ShaderLoader::LoadShader(name, filePath);
			if (!shader)
				CDL_CORE_PANIC(LogChannel::Render, "{}", shader.error());

			return std::move(*shader);
		}

		// TODO: Eventually move out of here once render grpah is implemented
		template<typename T>
		void ApplyPushConstants(vk::CommandBuffer commandBuffer, vk::PipelineLayout layout, const T& data)
		{
			CDL_STATIC_ASSERT(std::is_same_v<decltype(T::Global), GPU::GlobalPushConstants> && offsetof(T, Global) == 0,
				"A push-constant struct must start with 'GlobalPushConstants Global'");
			CDL_STATIC_ASSERT(sizeof(T) <= kPushConstantsSize, "Push-constant struct exceeds the engine-wide range");
			commandBuffer.pushConstants(layout, kPushConstantStages, 0, sizeof(T), &data);
		}
	}

	Renderer::Renderer(RenderDevice& renderDevice, GeometryPool& geometryPool, TexturePool& texturePool, BindlessTable& bindlessTable,
		GPUAllocator& allocator, const RendererCreationInfo& info)
		: m_RenderDevice(renderDevice)
		, m_GeometryPool(geometryPool)
		, m_TexturePool(texturePool)
		, m_BindlessTable(bindlessTable)
		, m_GPUAllocator(allocator)
		, m_PipelineStore(renderDevice, bindlessTable, info.PipelineCacheDirectory)
		, m_FrameGraph({ .Allocator = allocator, .BindlessTable = bindlessTable, .TexturePool = texturePool })
		, m_RenderToSwapChain(info.RenderToSwapChain)
		, m_TargetFormat(info.TargetFormat)
		, m_DepthFormat(FindDepthFormat(renderDevice))
		, m_LineWidthRange(FindLineWidthRange(renderDevice))
	{
		// Load shaders into pipeline for now 
		// TODO: Eventually will be part of the asset manager's initialization and will use asset handles rather than pointers
		m_TrianglePipeline = m_PipelineStore.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = LoadEngineShader("TrianglePipeline", info.ShaderDirectory / "Triangle.spv"),
			.VertexEntryPoint = "VertMain",
			.FragmentEntryPoint = "FragMain",
			.ColorAttachmentFormats = { info.TargetFormat },
			.DepthAttachmentFormat = m_DepthFormat,
			.DebugName = "Triangle Pipeline"
		});
		m_DebugDrawPipeline = m_PipelineStore.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = LoadEngineShader("DebugDrawPipeline", info.ShaderDirectory / "DebugDraw.spv"),
			.VertexEntryPoint = "VertMain",
			.FragmentEntryPoint = "FragMain",
			.Topology = PrimitiveTopology::LineList,
			.ColorAttachmentFormats = { info.TargetFormat },
			.DepthAttachmentFormat = m_DepthFormat,
			.DebugName = "Debug Draw Pipeline"
		});
		m_GuiPipeline = m_PipelineStore.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = LoadEngineShader("GuiPipeline", info.ShaderDirectory / "Gui.spv"),
			.VertexEntryPoint = "VertMain",
			.FragmentEntryPoint = "FragMain",
			.Blend = BlendMode::AlphaBlend,
			.ColorAttachmentFormats = { info.TargetFormat },
			.CullBackFaces = false,
			.DebugName = "GUI Pipeline"
		});

		// Test Compute shader that will perform operations to be used by the triangle pipeline. This is just a test for now, will be removed later
		m_ComputePipeline = m_PipelineStore.CreatePipeline(ComputePipelineSpecification{
			.Shader = LoadEngineShader("ComputePipeline", info.ShaderDirectory / "Triangle.spv"),
			.EntryPoint = "CompMain",
			.DebugName = "Test Compute Pipeline"
			});

		// Save the pipeline cache to disk so that it can be loaded next time the engine is run
		m_PipelineStore.SaveCache();
	}

	void Renderer::Render(const FramePacket& packet, FrameContext& frame, SwapChainImage& target)
	{
		CDL_PROFILE_SCOPE("Renderer::Render");

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
			.ViewProjection = packet.RenderView.Projection * packet.RenderView.View,
			.TransformData = frame.TransformData.GetDeviceAddress(),
			.TransformCount = static_cast<uint32_t>(transforms.size()),
			.Time = static_cast<float>(std::fmod(packet.Time, kShaderTimeWrapSeconds))
		};
		frame.FrameData.Write(std::as_bytes(std::span(&frameData, 1)));

		const GPU::GlobalPushConstants global{
			.Frame = frame.FrameData.GetDeviceAddress()
		};

		const glm::uvec2 sceneExtent = m_RenderToSwapChain ? target.GetDesc().Extent : m_TexturePool.GetTextureInfo(packet.OutputTarget).Desc.Extent;
		m_FrameGraph.Reset(sceneExtent);

		const FGTextureHandle swapChainHandle = m_FrameGraph.ImportSwapChainImage(target);
		const FGTextureHandle sceneColor = m_RenderToSwapChain ? swapChainHandle : m_FrameGraph.ImportTexture("OutputTarget", packet.OutputTarget);

		const ComputePassData& computeData = AddTestComputePass(global);
		const ScenePassData& sceneData = AddScenePass(packet, sceneColor, computeData.Output, global);
		AddDebugDrawPass(packet, frame, sceneData, global);
		AddGuiPass(packet, frame, swapChainHandle, sceneData.Color, global);

		m_FrameGraph.Compile(packet.FrameIndex);
		m_FrameGraph.EvictUnusedResources(packet.FrameIndex,
			m_RenderDevice.GetLastSubmittedTimelineValue(QueueType::Graphics),
			m_RenderDevice.GetCompletedTimelineValue(QueueType::Graphics));

		frame.CommandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		m_BindlessTable.BindSet(*frame.CommandBuffer, vk::PipelineBindPoint::eGraphics, m_PipelineStore.GetGlobalLayout());
		m_BindlessTable.BindSet(*frame.CommandBuffer, vk::PipelineBindPoint::eCompute, m_PipelineStore.GetGlobalLayout());

		m_FrameGraph.Execute(m_RenderDevice, frame.CommandBuffer);

		frame.CommandBuffer.end();

		// The final barrier left it ready to present; anything reading the tracked state before the next acquire, like a test's readback, expects that
		target.SetState(m_FrameGraph.GetTexture(swapChainHandle).FinalState);
	}

	const Renderer::ComputePassData& Renderer::AddTestComputePass(const GPU::GlobalPushConstants& global)
	{
		return m_FrameGraph.AddComputePass<ComputePassData>("ComputePass",
			[&](FrameGraphBuilder& builder, ComputePassData& data)
			{
				data.Output = builder.CreateTexture("ComputeOutput", { .Size = { .Mode = FGSizeMode::Absolute, .Extent = { 64, 64 } }, .Format = vk::Format::eR8G8B8A8Unorm });
				builder.WriteTexture(data.Output, FGUsage::StorageCompute);
			},
			[this, global](const ComputePassData& data, FrameGraphContext& ctx)
			{
				vk::Pipeline computePipeline = m_PipelineStore.TryGetPipeline(m_ComputePipeline);
				if (!computePipeline)
				{
					CDL_CORE_ASSERT(false, "Compute pipeline is not ready");
					return;
				}

				auto entryPoint = m_PipelineStore.GetEntryPoint(m_ComputePipeline, ShaderStage::Compute);
				if (!entryPoint)
				{
					CDL_CORE_ASSERT(false, "Failed to get compute pipeline entry point: {}", entryPoint.error());
					return;
				}

				const FGTextureEntry& texture = ctx.GetTexture(data.Output);
				vk::CommandBuffer commandBuffer = ctx.GetCommandBuffer();
				commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);

				const GPU::ComputePushConstants pushConstants{
					.Global = global,
					.TextureIndex = texture.BindlessIndex,
					.SamplerIndex = static_cast<uint32_t>(SamplerType::LinearClamp)
				};
				ApplyPushConstants(commandBuffer, m_PipelineStore.GetGlobalLayout(), pushConstants);

				const LocalGroupSize& groupSize = entryPoint->LocalSize;
				commandBuffer.dispatch((texture.Desc.Extent.x + groupSize.X - 1) / groupSize.X, (texture.Desc.Extent.y + groupSize.Y - 1) / groupSize.Y, 1);
			});
	}

	const Renderer::ScenePassData& Renderer::AddScenePass(const FramePacket& packet, FGTextureHandle target, FGTextureHandle computeTex, const GPU::GlobalPushConstants& global)
	{
		return m_FrameGraph.AddRasterPass<ScenePassData>("TrianglePass",
			[&](FrameGraphBuilder& builder, ScenePassData& data)
			{
				data.Color = target;
				data.Depth = builder.CreateTexture("Depth", { .Size = { .Mode = FGSizeMode::RelativeToOutput }, .Format = m_DepthFormat });
				builder.ColorAttachment(data.Color, 0, FGLoad::Clear, vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));
				builder.DepthAttachment(data.Depth, FGLoad::Clear);
				builder.ReadTexture(computeTex, FGUsage::SampledFragment);
			},
			[this, &packet, computeTex, global](const ScenePassData&, FrameGraphContext& ctx)
			{
				vk::Pipeline trianglePipeline = m_PipelineStore.TryGetPipeline(m_TrianglePipeline);
				if (!trianglePipeline)
				{
					CDL_CORE_ASSERT(false, "Triangle pipeline is not ready");
					return;
				}

				vk::CommandBuffer commandBuffer = ctx.GetCommandBuffer();
				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, trianglePipeline);

				const vk::Extent2D extent = ctx.GetRenderExtent();
				commandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(extent.height), static_cast<float>(extent.width), -static_cast<float>(extent.height), 0.0f, 1.0f));
				commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
				commandBuffer.setDepthTestEnable(true);

				const uint32_t computeTextureIndex = ctx.GetTexture(computeTex).BindlessIndex;
				for (const DrawItem& item : packet.DrawItems)
				{
					if (item.TransformIndex >= packet.TransformData.size())
					{
						CDL_CORE_ASSERT(false, "DrawItem's TransformIndex is past the packet's TransformData");
						continue;
					}

					MeshDrawInfo drawInfo = m_GeometryPool.GetMeshInfo(item.Mesh);
					TextureInfo textureInfo = m_TexturePool.GetTextureInfo(item.Texture);

					const GPU::MeshDrawPushConstants pushConstants{
						.Global = global,
						.Vertices = drawInfo.VerticesAddr,
						.TextureIndex = textureInfo.Index,
						.SamplerIndex = static_cast<uint32_t>(item.Sampler),
						.TransformIndex = item.TransformIndex,
						.ComputeTextureIndex = computeTextureIndex
					};

					ApplyPushConstants(commandBuffer, m_PipelineStore.GetGlobalLayout(), pushConstants);
					commandBuffer.bindIndexBuffer(drawInfo.Indices, 0, vk::IndexType::eUint32);
					commandBuffer.drawIndexed(drawInfo.IndexCount, 1, 0, 0, 0);
				}
			});
	}

	void Renderer::AddDebugDrawPass(const FramePacket& packet, FrameContext& frame, const ScenePassData& sceneData, const GPU::GlobalPushConstants& global)
	{
		if (packet.DebugDraw.Vertices.empty())
			return;

		m_FrameGraph.AddRasterPass("DebugDrawPass",
			[&](FrameGraphBuilder& builder, FGNoData&)
			{
				builder.ColorAttachment(sceneData.Color, 0, FGLoad::Load, {});
				builder.DepthAttachment(sceneData.Depth, FGLoad::Load);
			},
			[this, &packet, &frame, global](const FGNoData&, FrameGraphContext& ctx)
			{
				vk::Pipeline debugDrawPipeline = m_PipelineStore.TryGetPipeline(m_DebugDrawPipeline);
				if (!debugDrawPipeline)
				{
					CDL_CORE_ASSERT(false, "Debug draw pipeline is not ready");
					return;
				}

				const DebugDrawData& debugDraw = packet.DebugDraw;
				EnsureCapacity(frame.DebugDrawVertices, debugDraw.Vertices.size() * sizeof(GPU::DebugDrawVertex), vk::BufferUsageFlagBits::eShaderDeviceAddress, "DebugDrawVertices");
				frame.DebugDrawVertices.Write(std::as_bytes(std::span(debugDraw.Vertices)));

				vk::CommandBuffer commandBuffer = ctx.GetCommandBuffer();
				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, debugDrawPipeline);

				const vk::Extent2D extent = ctx.GetRenderExtent();
				commandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(extent.height), static_cast<float>(extent.width), -static_cast<float>(extent.height), 0.0f, 1.0f));
				commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

				const vk::DeviceAddress vertexBase = frame.DebugDrawVertices.GetDeviceAddress();
				std::optional<DebugDrawBucket> currentBucket;
				for (const DebugDrawBatch& batch : debugDraw.Batches)
				{
					if (batch.Bucket != currentBucket)
					{
						bool depthEnabled;
						switch (batch.Bucket)
						{
						case DebugDrawBucket::WorldDepthTested:
							depthEnabled = true;
							break;
						case DebugDrawBucket::WorldNoDepth:
						case DebugDrawBucket::Screen:
							depthEnabled = false;
							break;
						default:
							CDL_CORE_ASSERT(false, "Unhandled bucket type");
							continue;
						}

						commandBuffer.setDepthTestEnable(depthEnabled);
						currentBucket = batch.Bucket;

						// TODO: Some way to use world ViewProj matrix or a screen space matrix
					}

					commandBuffer.setLineWidth(std::clamp(batch.LineWidth, m_LineWidthRange.x, m_LineWidthRange.y));

					const GPU::DebugDrawPushConstants pushConstants{
						.Global = global,
						.Vertices = vertexBase + (batch.FirstVertex * sizeof(GPU::DebugDrawVertex))
					};

					ApplyPushConstants(commandBuffer, m_PipelineStore.GetGlobalLayout(), pushConstants);
					commandBuffer.draw(batch.VertexCount, 1, 0, 0);
				}
			});
	}

	void Renderer::AddGuiPass(const FramePacket& packet, FrameContext& frame, FGTextureHandle swapChain, FGTextureHandle sceneColor, const GPU::GlobalPushConstants& global)
	{
		const bool drawsOverScene = sceneColor == swapChain;

		// The scene pass has already cleared the target, so with nothing to draw this pass has nothing to do
		if (packet.GuiDrawData.Commands.empty() && drawsOverScene)
			return;

		m_FrameGraph.AddRasterPass("GuiPass",
			[&](FrameGraphBuilder& builder, FGNoData&)
			{
				// The scene window samples the output target through its bindless slot, which the graph can't see
				if (!drawsOverScene)
					builder.ReadTexture(sceneColor, FGUsage::SampledFragment);

				builder.ColorAttachment(swapChain, 0, drawsOverScene ? FGLoad::Load : FGLoad::Clear, vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));
			},
			[this, &packet, &frame, global](const FGNoData&, FrameGraphContext& ctx)
			{
				const GuiDrawData& gui = packet.GuiDrawData;
				if (gui.Commands.empty())
					return;

				vk::Pipeline guiPipeline = m_PipelineStore.TryGetPipeline(m_GuiPipeline);
				if (!guiPipeline)
				{
					CDL_CORE_ASSERT(false, "GUI pipeline is not ready");
					return;
				}

				EnsureCapacity(frame.GuiVertices, gui.Vertices.size() * sizeof(Candle::GPU::GuiVertex), vk::BufferUsageFlagBits::eShaderDeviceAddress, "GuiVertices");
				EnsureCapacity(frame.GuiIndices, gui.Indices.size() * sizeof(GuiIndex), vk::BufferUsageFlagBits::eIndexBuffer, "GuiIndices");
				frame.GuiVertices.Write(std::as_bytes(std::span(gui.Vertices)));
				frame.GuiIndices.Write(std::as_bytes(std::span(gui.Indices)));

				vk::CommandBuffer commandBuffer = ctx.GetCommandBuffer();
				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, guiPipeline);

				const vk::Extent2D extent = ctx.GetRenderExtent();
				commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f));
				commandBuffer.setDepthTestEnable(false);

				const glm::vec2 scale = 2.0f / gui.DisplaySize;
				const glm::vec2 translate = -1.0f - gui.DisplayPos * scale;

				commandBuffer.bindIndexBuffer(frame.GuiIndices.GetBuffer(), 0, vk::IndexType::eUint16);
				const vk::DeviceAddress vertexBase = frame.GuiVertices.GetDeviceAddress();

				const uint32_t samplerIndex = static_cast<uint32_t>(SamplerType::LinearClamp);
				for (const GuiDrawCommand& cmd : gui.Commands)
				{
					const glm::vec2 clipMin = glm::max(glm::vec2(cmd.ClipRect.x, cmd.ClipRect.y), glm::vec2(0.0f));
					const glm::vec2 clipMax = glm::min(glm::vec2(cmd.ClipRect.z, cmd.ClipRect.w), glm::vec2(extent.width, extent.height));
					if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
						continue;

					commandBuffer.setScissor(0, vk::Rect2D(
						vk::Offset2D(static_cast<int32_t>(clipMin.x), static_cast<int32_t>(clipMin.y)),
						vk::Extent2D(static_cast<uint32_t>(clipMax.x - clipMin.x), static_cast<uint32_t>(clipMax.y - clipMin.y))));

					const GPU::GuiDrawPushConstants pushConstants{
						.Global = global,
						.Vertices = vertexBase + cmd.VertexOffset * sizeof(Candle::GPU::GuiVertex),
						.Scale = scale,
						.Translate = translate,
						.TextureIndex = m_TexturePool.GetTextureInfo(cmd.Texture).Index,
						.SamplerIndex = samplerIndex
					};

					ApplyPushConstants(commandBuffer, m_PipelineStore.GetGlobalLayout(), pushConstants);
					commandBuffer.drawIndexed(cmd.IndexCount, 1, cmd.IndexOffset, 0, 0);
				}
			}
		);
	}

	void Renderer::EnsureCapacity(GPUBuffer& buffer, size_t size, vk::BufferUsageFlags usage, std::string_view name)
	{
		if (buffer.GetSize() >= size)
			return;

		buffer = m_GPUAllocator.CreateBuffer({
			.Size = std::max<size_t>(size, buffer.GetSize() * 2),   // Doubling, so a growing UI doesn't reallocate every frame
			.Usage = usage,
			.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.DebugName = std::string(name) 
		});
	}
}