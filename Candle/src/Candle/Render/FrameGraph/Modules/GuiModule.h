#pragma once

#include "Candle/Render/FrameGraph/RenderModule.h"
#include "Candle/Render/FrameGraph/FrameGraphHandles.h"

namespace Candle {

	class FrameGraph;
	class PipelineStore;
	class TexturePool;

	class GuiModule : public RenderModule
	{
	public:
		GuiModule(const RenderModuleSpec& spec, TexturePool& texturePool);
		virtual ~GuiModule() = default;

		void AddPasses(const RenderFrameInputs& inputs) override;
		inline const char* GetName() const override { return "GUI"; }

	private:
		struct GuiPassData
		{
			FGTextureHandle Color;
		};

	private:
		FrameGraph& m_FrameGraph;
		PipelineStore& m_PipelineStore;
		TexturePool& m_TexturePool;

		GPUPipelineHandle m_GuiPipeline;
	};

}