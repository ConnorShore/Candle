#pragma once

#include "Candle/Render/FrameGraph/RenderModule.h"
#include "Candle/Render/FrameGraph/FrameGraphHandles.h"

#include <glm/glm.hpp>

namespace Candle {

	class FrameGraph;
	class PipelineStore;
	class RenderDevice;
	class GPUAllocator;

	class DebugModule : public RenderModule
	{
	public:
		DebugModule(const RenderModuleSpec& spec, RenderDevice& renderDevice, GPUAllocator& allocator);
		virtual ~DebugModule() = default;

		void AddPasses(const RenderFrameInputs& inputs) override;
		inline const char* GetName() const override { return "Debug"; }

	private:
		struct DebugDrawPassData
		{
			FGTextureHandle Color;
			FGTextureHandle Depth;
		};

	private:
		FrameGraph& m_FrameGraph;
		PipelineStore& m_PipelineStore;
		GPUAllocator& m_GPUAllocator;

		GPUPipelineHandle m_DebugDrawPipeline;
		glm::vec2 m_LineWidthRange;

	};

}