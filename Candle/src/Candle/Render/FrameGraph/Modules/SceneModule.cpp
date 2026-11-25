#include "cdlpch.h"
#include "SceneModule.h"

#include "Candle/Render/FrameGraph/FrameGraph.h"
#include "Candle/Render/Pipelines/PipelineStore.h"
#include "Candle/Render/Pipelines/TriangleInterop.h"
#include "Candle/Render/Resources/GeometryPool.h"
#include "Candle/Render/Resources/TexturePool.h"


namespace Candle {

	SceneModule::SceneModule(const RenderModuleSpec& spec, GeometryPool& geometryPool, TexturePool& texturePool)
		: m_FrameGraph(spec.Graph)
		, m_PipelineStore(spec.Pipelines)
		, m_GeometryPool(geometryPool)
		, m_TexturePool(texturePool)
		, m_DepthFormat(spec.DepthFormat)
	{
		m_TrianglePipeline = spec.Pipelines.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = LoadEngineShader("TrianglePipeline", spec.ShaderDirectory / "Triangle.spv"),
			.VertexEntryPoint = "VertMain",
			.FragmentEntryPoint = "FragMain",
			.ColorAttachmentFormats = { spec.ColorFormat },
			.DepthAttachmentFormat = m_DepthFormat,
			.DebugName = "Triangle Pipeline"
		});

		m_ComputePipeline = spec.Pipelines.CreatePipeline(ComputePipelineSpecification{
			.Shader = LoadEngineShader("ComputePipeline", spec.ShaderDirectory / "Triangle.spv"),
			.EntryPoint = "CompMain",
			.DebugName = "Test Compute Pipeline"
		});
	}

	void SceneModule::AddPasses(const RenderFrameInputs& inputs)
	{
		Blackboard& blackboard = m_FrameGraph.GetBlackboard();

		const ComputePassData& computeData = AddTestComputePass(inputs);
		const SceneTextureData& sceneData = AddScenePass(inputs, computeData, blackboard);

		blackboard.Set<SceneTextureData>(sceneData);
	}

	const SceneModule::ComputePassData& SceneModule::AddTestComputePass(const RenderFrameInputs& inputs)
	{
		return m_FrameGraph.AddComputePass<ComputePassData>("ComputePass",
			[&](FrameGraphBuilder& builder, ComputePassData& data)
			{
				FGTextureHandle output = builder.CreateTexture("ComputeOutput", { .Size = { .Mode = FGSizeMode::Absolute, .Extent = { 64, 64 } }, .Format = vk::Format::eR8G8B8A8Unorm });
				data.Output = builder.WriteTexture(output, FGUsage::StorageWriteCompute);
			},
			[&](const ComputePassData& data, FrameGraphContext& ctx)
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
				const GPU::ComputePushConstants pushConstants{
					.Global = inputs.Global,
					.TextureIndex = texture.BindlessIndex,
					.SamplerIndex = static_cast<uint32_t>(SamplerType::LinearClamp)
				};
				ctx.BindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);
				ctx.PushConstants(pushConstants);

				const LocalGroupSize& groupSize = (*entryPoint)->LocalSize;
				ctx.Dispatch2D((texture.Desc.Extent.x + groupSize.X - 1) / groupSize.X, (texture.Desc.Extent.y + groupSize.Y - 1) / groupSize.Y);
			});
	}

	const SceneTextureData& SceneModule::AddScenePass(const RenderFrameInputs& inputs, const ComputePassData& computeData, Blackboard& blackboard)
	{
		return m_FrameGraph.AddRasterPass<SceneTextureData>("TrianglePass",
			[&](FrameGraphBuilder& builder, SceneTextureData& data)
			{
				FrameTargetData& frameTargetData = blackboard.Get<FrameTargetData>();

				FGTextureHandle depth = builder.CreateTexture("Depth", { .Size = { .Mode = FGSizeMode::RelativeToOutput }, .Format = m_DepthFormat });

				data.Color = builder.ColorAttachment(frameTargetData.Output, FGLoad::Clear, vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));
				data.Depth = builder.DepthAttachment(depth, FGLoad::Clear);
				builder.ReadTexture(computeData.Output, FGUsage::SampledFragment);
			},
			[&](const SceneTextureData& data, FrameGraphContext& ctx)
			{
				vk::Pipeline trianglePipeline = m_PipelineStore.TryGetPipeline(m_TrianglePipeline);
				if (!trianglePipeline)
				{
					CDL_CORE_ASSERT(false, "Triangle pipeline is not ready");
					return;
				}

				ctx.BindPipeline(vk::PipelineBindPoint::eGraphics, trianglePipeline);
				ctx.EnableDepthTest(true);

				const uint32_t computeTextureIndex = ctx.GetTexture(computeData.Output).BindlessIndex;
				for (const DrawItem& item : inputs.Packet.DrawItems)
				{
					if (item.TransformIndex >= inputs.Packet.TransformData.size())
					{
						CDL_CORE_ASSERT(false, "DrawItem's TransformIndex is past the packet's TransformData");
						continue;
					}

					MeshDrawInfo drawInfo = m_GeometryPool.GetMeshInfo(item.Mesh);
					TextureInfo textureInfo = m_TexturePool.GetTextureInfo(item.Texture);

					const GPU::MeshDrawPushConstants pushConstants{
						.Global = inputs.Global,
						.Vertices = drawInfo.VerticesAddr,
						.TextureIndex = textureInfo.Index,
						.SamplerIndex = static_cast<uint32_t>(item.Sampler),
						.TransformIndex = item.TransformIndex,
						.ComputeTextureIndex = computeTextureIndex
					};

					ctx.PushConstants(pushConstants);
					ctx.BindIndexBuffer(drawInfo.Indices, 0, vk::IndexType::eUint32);
					ctx.DrawIndexed(drawInfo.IndexCount, 1, 0, 0, 0);
				}
			}, true);
	}

}