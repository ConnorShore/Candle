#pragma once

#include "FrameGraphHandles.h"
#include "FrameGraphResources.h"

#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"
#include "Candle/Render/Pipelines/PipelineConstants.h"

#include <vulkan/vulkan_raii.hpp>

#include <string_view>

namespace Candle {

	class FrameGraph;
	class PipelineStore;
	struct FGTextureEntry;

	// Built by FrameGraph::Execute for each pass; valid only inside that pass's execute
	class FrameGraphContext
	{
	public:
		const FGTextureEntry& GetTexture(FGTextureHandle handle) const; // Asserts this pass declared it
		vk::Extent2D GetRenderExtent() const;   // Of this pass's attachments, for viewport and scissor
		std::string_view GetPassName() const;

		void BindPipeline(vk::PipelineBindPoint bindPoint, vk::Pipeline pipeline);

		inline vk::CommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }


		template<typename T>
		inline void PushConstants(const T& data)
		{
			CDL_STATIC_ASSERT(std::is_same_v<decltype(T::Global), GPU::GlobalPushConstants> && offsetof(T, Global) == 0,
				"A push-constant struct must start with 'GlobalPushConstants Global'");
			CDL_STATIC_ASSERT(sizeof(T) <= kPushConstantsSize, "Push-constant struct exceeds the engine-wide range");
			m_CommandBuffer.pushConstants(m_GlobalPipelineLayout, kPushConstantStages, 0, sizeof(T), &data);
		}

	private:
		// Private so only FrameGraph can constuct it
		FrameGraphContext(const FrameGraph& frameGraph, const PipelineStore& pipelineStore, vk::CommandBuffer commandBuffer, uint32_t passIndex);

		friend class FrameGraph;

	private:
		const FrameGraph& m_FrameGraph;
		uint32_t m_PassIndex;
		vk::CommandBuffer m_CommandBuffer;
		vk::PipelineLayout m_GlobalPipelineLayout;
	};

}