#include "cdlpch.h"
#include "FrameGraphContext.h"
#include "FrameGraph.h"

#include <glm/glm.hpp>

namespace Candle {

	// Private
	FrameGraphContext::FrameGraphContext(const FrameGraph& frameGraph, vk::CommandBuffer commandBuffer, uint32_t passIndex)
		: m_FrameGraph(frameGraph), m_CommandBuffer(commandBuffer), m_PassIndex(passIndex)
	{
	}

	const FGTextureEntry& FrameGraphContext::GetTexture(FGTextureHandle handle) const
	{
		return m_FrameGraph.m_Textures[handle.Index];
	}

	vk::Extent2D FrameGraphContext::GetRenderExtent() const
	{
		vk::Extent2D extent{
			.width = 0,
			.height = 0
		};
		for (const FGTextureAccessEntry& entry : m_FrameGraph.m_TextureAccesses[m_PassIndex])
		{
			const FGTextureEntry& texture = m_FrameGraph.m_Textures[entry.Handle.Index];
			if (entry.Usage == FGUsage::ColorAttachment || entry.Usage == FGUsage::DepthAttachment)
			{
				extent.width = glm::max(extent.width, static_cast<uint32_t>(texture.Desc.Extent.x));
				extent.height = glm::max(extent.height, static_cast<uint32_t>(texture.Desc.Extent.y));
			}
		}
		return extent;
	}

	std::string_view FrameGraphContext::GetPassName() const
	{
		return m_FrameGraph.m_Passes[m_PassIndex].Name;
	}

}