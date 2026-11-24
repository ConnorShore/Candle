#pragma once

#include "GlobalModuleData.h"

#include "Candle/Render/FrameGraph/RenderModule.h"
#include "Candle/Render/FrameGraph/FrameGraphHandles.h"

#include <vulkan/vulkan.hpp>

namespace Candle {

	class FrameGraph;
	class GeometryPool;
	class TexturePool;
	class Blackboard;
	class PipelineStore;

	class SceneModule : public RenderModule
	{
	public:
		SceneModule(const RenderModuleSpec& spec, GeometryPool& geometryPool, TexturePool& texturePool);
		virtual ~SceneModule() = default;

		void AddPasses(const RenderFrameInputs& inputs) override;
		inline const char* GetName() const override { return "Scene"; }

	private:
		struct ComputePassData
		{
			FGTextureHandle Output;
		};

		const ComputePassData& AddTestComputePass(const RenderFrameInputs& inputs);
		const SceneTextureData& AddScenePass(const RenderFrameInputs& inputs, const ComputePassData& computeData, Blackboard& blackboard);

	private:
		FrameGraph& m_FrameGraph;
		PipelineStore& m_PipelineStore;
		GeometryPool& m_GeometryPool;
		TexturePool& m_TexturePool;

		GPUPipelineHandle m_TrianglePipeline, m_ComputePipeline;
		vk::Format m_DepthFormat;
	};

}