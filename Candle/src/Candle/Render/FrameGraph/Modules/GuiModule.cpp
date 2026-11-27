#include "cdlpch.h"
#include "GuiModule.h"
#include "GlobalModuleData.h"

#include "Candle/Render/FrameGraph/FrameGraph.h"
#include "Candle/Render/Pipelines/PipelineStore.h"
#include "Candle/Render/Resources/GPUArenaAllocator.h"
#include "Candle/Render/Resources/TexturePool.h"
#include "Candle/Render/GUI/GuiInterop.h"

namespace Candle {

	GuiModule::GuiModule(const RenderModuleSpec& spec, TexturePool& texturePool)
		: m_FrameGraph(spec.Graph)
		, m_PipelineStore(spec.Pipelines)
		, m_TexturePool(texturePool)
	{
		m_GuiPipeline = m_PipelineStore.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = LoadEngineShader("GuiPipeline", spec.ShaderDirectory / "Gui.spv"),
			.VertexEntryPoint = "VertMain",
			.FragmentEntryPoint = "FragMain",
			.Blend = BlendMode::AlphaBlend,
			.ColorAttachmentFormats = { spec.ColorFormat },
			.CullBackFaces = false,
			.DebugName = "GUI Pipeline"
		});
	}

	void GuiModule::AddPasses(const RenderFrameInputs& inputs)
	{
		Blackboard& blackboard = m_FrameGraph.GetBlackboard();
		FrameTargetData& frameTargets = blackboard.Get<FrameTargetData>();
		SceneTextureData& sceneData = blackboard.Get<SceneTextureData>();

		const bool drawsOverScene = frameTargets.Output.IsSameResource(frameTargets.SwapChain);

		// The scene pass has already cleared the target, so with nothing to draw this pass has nothing to do
		if (inputs.Packet.GuiDrawData.Commands.empty() && drawsOverScene)
			return;

		GuiPassData pass = m_FrameGraph.AddRasterPass<GuiPassData>("GuiPass",
			[&](FrameGraphBuilder& builder, GuiPassData& data)
			{
				// The scene window samples the output target through its bindless slot, which the graph can't see
				if (!drawsOverScene)
					builder.ReadTexture(sceneData.Color, FGUsage::SampledFragment);

				// The render graph viewer shows the preview the same way
				if (const FrameGraphPreviewData* preview = blackboard.TryGet<FrameGraphPreviewData>())
					builder.ReadTexture(preview->Target, FGUsage::SampledFragment);

				// When drawing over the scene, sceneColor is the same image as swapChain but carries the latest version
				FGTextureHandle target = drawsOverScene ? sceneData.Color : frameTargets.SwapChain;
				data.Color = builder.ColorAttachment(target, drawsOverScene ? FGLoad::Load : FGLoad::Clear, vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f));
			},
			[&](const GuiPassData& data, FrameGraphContext& ctx)
			{
				const GuiDrawData& gui = inputs.Packet.GuiDrawData;
				if (gui.Commands.empty())
					return;

				vk::Pipeline guiPipeline = m_PipelineStore.TryGetPipeline(m_GuiPipeline);
				if (!guiPipeline)
				{
					CDL_CORE_ASSERT(false, "GUI pipeline is not ready");
					return;
				}

				inputs.Frame.GuiVertices = inputs.Frame.FrameArena.Push(std::as_bytes(std::span(gui.Vertices)));
				inputs.Frame.GuiIndices = inputs.Frame.FrameArena.Push(std::as_bytes(std::span(gui.Indices)));

				ctx.BindPipeline(vk::PipelineBindPoint::eGraphics, guiPipeline);
				ctx.EnableDepthTest(false);

				// Override viewport so not upside down
				const vk::Extent2D extent = ctx.GetRenderExtent();
				ctx.SetViewport(0, 0, extent.width, extent.height, 0.0f, 1.0f);

				const glm::vec2 scale = 2.0f / gui.DisplaySize;
				const glm::vec2 translate = -1.0f - gui.DisplayPos * scale;

				ctx.BindIndexBuffer(inputs.Frame.GuiIndices.Buffer, inputs.Frame.GuiIndices.Offset, vk::IndexType::eUint16);
				const vk::DeviceAddress vertexBase = inputs.Frame.GuiVertices.Address;

				const uint32_t samplerIndex = static_cast<uint32_t>(SamplerType::LinearClamp);
				for (const GuiDrawCommand& cmd : gui.Commands)
				{
					const glm::vec2 clipMin = glm::max(glm::vec2(cmd.ClipRect.x, cmd.ClipRect.y), glm::vec2(0.0f));
					const glm::vec2 clipMax = glm::min(glm::vec2(cmd.ClipRect.z, cmd.ClipRect.w), glm::vec2(extent.width, extent.height));
					if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
						continue;

					ctx.SetScissor(clipMin, clipMax - clipMin);

					const GPU::GuiDrawPushConstants pushConstants{
						.Global = inputs.Global,
						.Vertices = vertexBase + cmd.VertexOffset * sizeof(Candle::GPU::GuiVertex),
						.Scale = scale,
						.Translate = translate,
						.TextureIndex = m_TexturePool.GetTextureInfo(cmd.Texture).Index,
						.SamplerIndex = samplerIndex
					};
					ctx.PushConstants(pushConstants);

					ctx.DrawIndexed(cmd.IndexCount, 1, cmd.IndexOffset, 0, 0);
				}
			}
		);
	}

}