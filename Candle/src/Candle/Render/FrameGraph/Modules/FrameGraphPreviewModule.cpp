#include "cdlpch.h"
#include "FrameGraphPreviewModule.h"
#include "GlobalModuleData.h"

#include "Candle/Render/FrameGraph/FrameGraph.h"
#include "Candle/Render/FrameGraph/FrameGraphCapture.h"
#include "Candle/Render/Pipelines/PipelineStore.h"
#include "Candle/Render/Pipelines/FrameGraphPreviewInterop.h"
#include "Candle/Render/Vulkan/SamplerType.h"

#include <vulkan/vulkan_format_traits.hpp>

#include <string_view>

namespace Candle {

	namespace {

		constexpr std::string_view kPreviewTargetName = "FrameGraphPreviewTarget";

		bool CanPreview(const FGTextureEntry& texture, FGTextureHandle handle)
		{
			return !texture.Imported && handle.Version > 0 && IsPreviewableFormat(texture.Desc.Format);
		}

	}

	FrameGraphPreviewModule::FrameGraphPreviewModule(const RenderModuleSpec& spec)
		: m_FrameGraph(spec.Graph)
		, m_PipelineStore(spec.Pipelines)
	{
		m_PreviewPipeline = m_PipelineStore.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = LoadEngineShader("FrameGraphPreviewPipeline", spec.ShaderDirectory / "FrameGraphPreview.spv"),
			.VertexEntryPoint = "VertMain",
			.FragmentEntryPoint = "FragMain",
			.ColorAttachmentFormats = { spec.ColorFormat },
			.CullBackFaces = false,
			.DebugName = "Frame Graph Preview Pipeline"
		});
	}

	void FrameGraphPreviewModule::AddPasses(const RenderFrameInputs& inputs)
	{
		const FrameGraphCaptureRequest& request = inputs.Packet.FrameGraphCapture;
		if (!request.PreviewTarget.IsValid())
			return;

		const std::optional<FGTextureHandle> source = m_FrameGraph.FindTexture(request.PreviewTexture);
		if (!source || !CanPreview(m_FrameGraph.GetTexture(*source), *source))
			return;

		const FGTextureHandle target = m_FrameGraph.ImportTexture(kPreviewTargetName, request.PreviewTarget);
		const bool isDepth = vk::hasDepthComponent(m_FrameGraph.GetTexture(*source).Desc.Format);

		// Execute runs after AddPasses returns, so it captures by value, apart from inputs, which lives for the whole frame
		const PreviewPassData& pass = m_FrameGraph.AddRasterPass<PreviewPassData>(kFrameGraphPreviewPassName,
			[&](FrameGraphBuilder& builder, PreviewPassData& data)
			{
				builder.ReadTexture(*source, FGUsage::SampledFragment);
				data.Source = *source;
				data.Target = builder.ColorAttachment(target, FGLoad::DontCare, {});
			},
			[this, &inputs, range = request.PreviewRange, isDepth](const PreviewPassData& data, FrameGraphContext& ctx)
			{
				vk::Pipeline previewPipeline = m_PipelineStore.TryGetPipeline(m_PreviewPipeline);
				if (!previewPipeline)
				{
					CDL_CORE_ASSERT(false, "Frame graph preview pipeline is not ready");
					return;
				}

				ctx.BindPipeline(vk::PipelineBindPoint::eGraphics, previewPipeline);
				ctx.EnableDepthTest(false);

				// Unflipped, so UV (0, 0) lands on the target's top-left texel, the way the GUI shows it
				const vk::Extent2D extent = ctx.GetRenderExtent();
				ctx.SetViewport(0, 0, extent.width, extent.height, 0.0f, 1.0f);

				const GPU::FrameGraphPreviewPushConstants pushConstants{
					.Global = inputs.Global,
					.TextureIndex = ctx.GetTexture(data.Source).BindlessIndex,
					.SamplerIndex = static_cast<uint32_t>(SamplerType::PointClamp),
					.Range = range,
					.IsDepth = isDepth ? 1u : 0u
				};
				ctx.PushConstants(pushConstants);
				ctx.Draw(3, 1, 0, 0);
			});

		m_FrameGraph.GetBlackboard().Set<FrameGraphPreviewData>(FrameGraphPreviewData{ .Target = pass.Target });
	}

}
