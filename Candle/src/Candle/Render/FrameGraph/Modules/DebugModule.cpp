#include "cdlpch.h"
#include "DebugModule.h"
#include "GlobalModuleData.h"

#include "Candle/Render/FrameGraph/FrameGraph.h"
#include "Candle/Render/Pipelines/PipelineStore.h"
#include "Candle/Render/Pipelines/TriangleInterop.h"
#include "Candle/Render/Pipelines/DebugDrawInterop.h"
#include "Candle/Render/Vulkan/RenderDevice.h"
#include "Candle/Render/Vulkan/GPUAllocator.h"

namespace Candle {

	namespace {

		// The device reports its range whether or not wideLines is enabled, but without it only 1 is valid
		glm::vec2 FindLineWidthRange(RenderDevice& device)
		{
			if (!device.HasWideLines())
				return { 1.0f, 1.0f };

			const auto& range = device.GetPhysicalDevice().getProperties().limits.lineWidthRange;
			return { range[0], range[1] };
		}

		// TODO NOW: Figure out way to make this a shared utility function
		void EnsureCapacity(GPUAllocator& allocator,GPUBuffer& buffer, size_t size, vk::BufferUsageFlags usage, std::string_view name)
		{
			if (buffer.GetSize() >= size)
				return;

			buffer = allocator.CreateBuffer({
				.Size = std::max<size_t>(size, buffer.GetSize() * 2),   // Doubling, so a growing UI doesn't reallocate every frame
				.Usage = usage,
				.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				.DebugName = std::string(name)
				});
		}

	}

	DebugModule::DebugModule(const RenderModuleSpec& spec, RenderDevice& renderDevice, GPUAllocator& allocator)
		: m_FrameGraph(spec.Graph)
		, m_PipelineStore(spec.Pipelines)
		, m_LineWidthRange(FindLineWidthRange(renderDevice))
		, m_GPUAllocator(allocator)
	{
		m_DebugDrawPipeline = spec.Pipelines.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = LoadEngineShader("DebugDrawPipeline", spec.ShaderDirectory / "DebugDraw.spv"),
			.VertexEntryPoint = "VertMain",
			.FragmentEntryPoint = "FragMain",
			.Topology = PrimitiveTopology::LineList,
			.ColorAttachmentFormats = { spec.ColorFormat },
			.DepthAttachmentFormat = spec.DepthFormat,
			.DebugName = "Debug Draw Pipeline"
		});
	}

	void DebugModule::AddPasses(const RenderFrameInputs& inputs)
	{
		Blackboard& blackboard = m_FrameGraph.GetBlackboard();
		SceneTextureData& sceneData = blackboard.Get<SceneTextureData>();

		if (inputs.Packet.DebugDraw.Vertices.empty())
			return;

		DebugDrawPassData pass = m_FrameGraph.AddRasterPass<DebugDrawPassData>("DebugDrawPass",
			[&](FrameGraphBuilder& builder, DebugDrawPassData& data)
			{
				data.Color = builder.ColorAttachment(sceneData.Color, FGLoad::Load, {});
				data.Depth = builder.DepthAttachment(sceneData.Depth, FGLoad::Load);
			},
			[&](const DebugDrawPassData& data, FrameGraphContext& ctx)
			{
				vk::Pipeline debugDrawPipeline = m_PipelineStore.TryGetPipeline(m_DebugDrawPipeline);
				if (!debugDrawPipeline)
				{
					CDL_CORE_ASSERT(false, "Debug draw pipeline is not ready");
					return;
				}

				// TODO: Will probably be handled be linear allocator for frame data
				const DebugDrawData& debugDraw = inputs.Packet.DebugDraw;
				EnsureCapacity(m_GPUAllocator, inputs.Frame.DebugDrawVertices, debugDraw.Vertices.size() * sizeof(GPU::DebugDrawVertex), vk::BufferUsageFlagBits::eShaderDeviceAddress, "DebugDrawVertices");
				inputs.Frame.DebugDrawVertices.Write(std::as_bytes(std::span(debugDraw.Vertices)));

				vk::CommandBuffer commandBuffer = ctx.GetCommandBuffer();
				ctx.BindPipeline(vk::PipelineBindPoint::eGraphics, debugDrawPipeline);

				const vk::DeviceAddress vertexBase = inputs.Frame.DebugDrawVertices.GetDeviceAddress();
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
						.Global = inputs.Global,
						.Vertices = vertexBase + (batch.FirstVertex * sizeof(GPU::DebugDrawVertex))
					};
					ctx.PushConstants(pushConstants);

					commandBuffer.draw(batch.VertexCount, 1, 0, 0);
				}
			}
		);

		// Update the blackboard with the new attachments for the next passes
		sceneData.Color = pass.Color;
		sceneData.Depth = pass.Depth;
	}

}