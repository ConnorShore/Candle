#include "cdlpch.h"
#include "DebugModule.h"
#include "GlobalModuleData.h"

#include "Candle/Render/FrameGraph/FrameGraph.h"
#include "Candle/Render/Pipelines/PipelineStore.h"
#include "Candle/Render/Pipelines/TriangleInterop.h"
#include "Candle/Render/Pipelines/DebugDrawInterop.h"
#include "Candle/Render/Vulkan/RenderDevice.h"
#include "Candle/Render/Resources/GPUArenaAllocator.h"

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

	}

	DebugModule::DebugModule(const RenderModuleSpec& spec, RenderDevice& renderDevice)
		: m_FrameGraph(spec.Graph)
		, m_PipelineStore(spec.Pipelines)
		, m_LineWidthRange(FindLineWidthRange(renderDevice))
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

				const DebugDrawData& debugDraw = inputs.Packet.DebugDraw;
				inputs.Frame.DebugDrawVertices = inputs.Frame.FrameArena.Push(std::as_bytes(std::span(debugDraw.Vertices)));

				vk::CommandBuffer commandBuffer = ctx.GetCommandBuffer();
				ctx.BindPipeline(vk::PipelineBindPoint::eGraphics, debugDrawPipeline);

				const vk::DeviceAddress vertexBase = inputs.Frame.DebugDrawVertices.Address;
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

						ctx.EnableDepthTest(depthEnabled);
						currentBucket = batch.Bucket;

						// TODO: Some way to use world ViewProj matrix or a screen space matrix
					}

					ctx.SetLineWidth(std::clamp(batch.LineWidth, m_LineWidthRange.x, m_LineWidthRange.y));

					const GPU::DebugDrawPushConstants pushConstants{
						.Global = inputs.Global,
						.Vertices = vertexBase + (batch.FirstVertex * sizeof(GPU::DebugDrawVertex))
					};
					ctx.PushConstants(pushConstants);

					ctx.Draw(batch.VertexCount, 1, 0, 0);
				}
			}
		);

		// Update the blackboard with the new attachments for the next passes
		sceneData.Color = pass.Color;
		sceneData.Depth = pass.Depth;
	}

}