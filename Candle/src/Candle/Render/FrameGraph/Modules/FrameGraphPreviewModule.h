#pragma once

#include "Candle/Render/FrameGraph/RenderModule.h"
#include "Candle/Render/FrameGraph/FrameGraphHandles.h"

namespace Candle {

	class FrameGraph;
	class PipelineStore;

	// Draws the transient the render graph viewer selected into the packet's preview target, and hands the target to the
	// GUI through the blackboard. Adds nothing unless the packet asks for a preview.
	class FrameGraphPreviewModule : public RenderModule
	{
	public:
		explicit FrameGraphPreviewModule(const RenderModuleSpec& spec);
		virtual ~FrameGraphPreviewModule() = default;

		void AddPasses(const RenderFrameInputs& inputs) override;
		inline const char* GetName() const override { return "FrameGraphPreview"; }

	private:
		struct PreviewPassData
		{
			FGTextureHandle Source;
			FGTextureHandle Target;
		};

	private:
		FrameGraph& m_FrameGraph;
		PipelineStore& m_PipelineStore;

		GPUPipelineHandle m_PreviewPipeline;
	};

}
