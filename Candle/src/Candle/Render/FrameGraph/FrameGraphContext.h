#pragma once

#include "FrameGraphHandles.h"
#include "FrameGraphResources.h"

#include <vulkan/vulkan_raii.hpp>

#include <string_view>

namespace Candle {

	class FrameGraph;
	struct FGTextureEntry;

	// Built by FrameGraph::Execute for each pass; valid only inside that pass's execute
	class FrameGraphContext
	{
	public:
		const FGTextureEntry& GetTexture(FGTextureHandle handle) const; // Asserts this pass declared it
		vk::Extent2D GetRenderExtent() const;   // Of this pass's attachments, for viewport and scissor
		std::string_view GetPassName() const;

		inline vk::CommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

	private:
		// Private so only FrameGraph can constuct it
		FrameGraphContext(const FrameGraph& frameGraph, vk::CommandBuffer commandBuffer, uint32_t passIndex);

		friend class FrameGraph;

	private:
		const FrameGraph& m_FrameGraph;
		uint32_t m_PassIndex;
		vk::CommandBuffer m_CommandBuffer;
	};

}