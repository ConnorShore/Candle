#include "cdlpch.h"
#include "FrameGraphContext.h"
#include "FrameGraph.h"

#include "Candle/Render/Pipelines/PipelineStore.h"

#include <glm/glm.hpp>

namespace Candle {

	// Private
	FrameGraphContext::FrameGraphContext(const FrameGraph& frameGraph, const PipelineStore& pipelineStore, vk::CommandBuffer commandBuffer, uint32_t passIndex)
		: m_FrameGraph(frameGraph), m_GlobalPipelineLayout(pipelineStore.GetGlobalLayout()), m_CommandBuffer(commandBuffer), m_PassIndex(passIndex)
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

	void FrameGraphContext::BindPipeline(vk::PipelineBindPoint bindPoint, vk::Pipeline pipeline)
	{
		m_CommandBuffer.bindPipeline(bindPoint, pipeline);
	}

	void FrameGraphContext::EnableDepthTest(bool enable)
	{
		m_CommandBuffer.setDepthTestEnable(enable);
	}

	void FrameGraphContext::SetLineWidth(float width)
	{
		m_CommandBuffer.setLineWidth(width);
	}

	void FrameGraphContext::SetViewport(uint32_t minX, uint32_t minY, uint32_t width, uint32_t height, float minDepth, float maxDepth)
	{
		m_CommandBuffer.setViewport(0, vk::Viewport(static_cast<float>(minX), static_cast<float>(minY), static_cast<float>(width), static_cast<float>(height), minDepth, maxDepth));
	}

	void FrameGraphContext::SetScissor(uint32_t minX, uint32_t minY, uint32_t width, uint32_t height)
	{
		m_CommandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(static_cast<int32_t>(minX), static_cast<int32_t>(minY)), vk::Extent2D(width, height)));
	}

	void FrameGraphContext::SetScissor(glm::vec2 offset, glm::vec2 extent)
	{
		SetScissor(static_cast<uint32_t>(offset.x), static_cast<uint32_t>(offset.y), static_cast<uint32_t>(extent.x), static_cast<uint32_t>(extent.y));
	}

	void FrameGraphContext::BindIndexBuffer(vk::Buffer buffer, vk::DeviceSize offset, vk::IndexType indexType)
	{
		m_CommandBuffer.bindIndexBuffer(buffer, offset, indexType);

	}

	void FrameGraphContext::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		m_CommandBuffer.draw(vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void FrameGraphContext::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		m_CommandBuffer.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void FrameGraphContext::Dispatch2D(uint32_t groupCountX, uint32_t groupCountY)
	{
		m_CommandBuffer.dispatch(groupCountX, groupCountY, 1);
	}

	void FrameGraphContext::Dispatch3D(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		m_CommandBuffer.dispatch(groupCountX, groupCountY, groupCountZ);
	}

}