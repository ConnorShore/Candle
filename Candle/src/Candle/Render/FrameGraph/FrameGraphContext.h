#pragma once

#include "FrameGraphHandles.h"
#include "FrameGraphResources.h"

#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"
#include "Candle/Render/Pipelines/PipelineConstants.h"

#include <vulkan/vulkan_raii.hpp>

#include <string_view>
#include <cstdint>

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

		void EnableDepthTest(bool enable);
		void SetLineWidth(float width);
		void SetViewport(uint32_t minX, uint32_t minY, uint32_t width, uint32_t height, float minDepth, float maxDepth);
		void SetScissor(uint32_t minX, uint32_t minY, uint32_t width, uint32_t height);
		void SetScissor(glm::vec2 offset, glm::vec2 extent);

		void BindPipeline(vk::PipelineBindPoint bindPoint, vk::Pipeline pipeline);
		void BindIndexBuffer(vk::Buffer buffer, vk::DeviceSize offset, vk::IndexType indexType);

		void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
		void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

		void Dispatch2D(uint32_t groupCountX, uint32_t groupCountY);
		void Dispatch3D(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

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