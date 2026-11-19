#include "cdlpch.h"
#include "FrameGraphBuilder.h"
#include "FrameGraph.h"
#include "FrameGraphPass.h"
#include "FrameGraphResources.h"

namespace Candle {

	FrameGraphBuilder::FrameGraphBuilder(FrameGraph& graph, uint32_t passIndex)
		: m_Graph(graph), m_PassIndex(passIndex) 
	{
	}

	FGTextureHandle FrameGraphBuilder::CreateTexture(std::string_view name, FGTextureSpec spec)
	{
		return m_Graph.CreateTexture(name, spec);
	}

	void FrameGraphBuilder::ReadTexture(FGTextureHandle texture, FGUsage usage)
	{
		// Add an entry to the pass's texture access list, which the graph will use to insert barriers and track usage
		FGTextureAccessEntry accessEntry{
			.Handle = texture,
			.Usage = usage,
			.Write = false
		};
		m_Graph.m_TextureAccesses[m_PassIndex].push_back(accessEntry);
	}

	void FrameGraphBuilder::WriteTexture(FGTextureHandle texture, FGUsage usage)
	{
		// Add an entry to the pass's texture access list, which the graph will use to insert barriers and track usage
		FGTextureAccessEntry accessEntry{
			.Handle = texture,
			.Usage = usage,
			.Write = true
		};
		m_Graph.m_TextureAccesses[m_PassIndex].push_back(accessEntry);
	}

	void FrameGraphBuilder::ColorAttachment(FGTextureHandle texture, uint8_t binding, FGLoad loadOp, vk::ClearColorValue clearColor)
	{
		// Add an entry to the pass's texture access list, which the graph will use to insert barriers and track usage
		FGTextureAccessEntry accessEntry{
			.Handle = texture,
			.Usage = FGUsage::ColorAttachment,
			.Write = true,
			.LoadOp = loadOp,
			.ClearVlaue = clearColor
		};
		m_Graph.m_TextureAccesses[m_PassIndex].push_back(accessEntry);

		// TODO: May not need binding here but will see once the graph is executing passes and binding attachments
		//  For now assuming binding gets set by order of specification in pass
	}

	void FrameGraphBuilder::DepthAttachment(FGTextureHandle texture, FGLoad loadOp)
	{
		FGTextureAccessEntry accessEntry{
			.Handle = texture,
			.Usage = FGUsage::DepthAttachment,
			.Write = true,
			.LoadOp = loadOp,
			.ClearVlaue = vk::ClearDepthStencilValue(1.0f, 0)	// TODO: Depth clear value should be specified by the pass, but for now just using a default
		};
		m_Graph.m_TextureAccesses[m_PassIndex].push_back(accessEntry);
	}

}