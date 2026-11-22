#include "cdlpch.h"
#include "FrameGraphBuilder.h"
#include "FrameGraph.h"
#include "FrameGraphPass.h"
#include "FrameGraphResources.h"

#include <glm/glm.hpp>

namespace Candle {

	namespace {

		glm::uvec2 ResolveSize(FGSize size, glm::uvec2 outputExtent)
		{
			switch (size.Mode)
			{
			case FGSizeMode::RelativeToOutput:
				return glm::uvec2(static_cast<uint32_t>(outputExtent.x * size.Scale), static_cast<uint32_t>(outputExtent.y * size.Scale));
			case FGSizeMode::Absolute:
				return size.Extent;
			default:
				CDL_CORE_ASSERT(false, "Unknown FGSizeMode");
				return glm::uvec2(0, 0);
			}
		}

		// TODO NOW: Consolodate to common location for other files to use
		bool IsDepthFormat(vk::Format format)
		{
			switch (format)
			{
			case vk::Format::eD16Unorm:
			case vk::Format::eX8D24UnormPack32:
			case vk::Format::eD32Sfloat:
			case vk::Format::eD16UnormS8Uint:
			case vk::Format::eD24UnormS8Uint:
			case vk::Format::eD32SfloatS8Uint:
				return true;
			default:
				return false;
			}
		}

	}

	FrameGraphBuilder::FrameGraphBuilder(FrameGraph& graph, uint32_t passIndex)
		: m_Graph(graph), m_PassIndex(passIndex) 
	{
	}

	FGTextureHandle FrameGraphBuilder::CreateTexture(std::string_view name, FGTextureSpec spec)
	{
		ImageState initialState{
			.Layout = vk::ImageLayout::eUndefined,
			.Access = vk::AccessFlagBits2::eNone,
			.Stage = vk::PipelineStageFlagBits2::eTopOfPipe
		};

		// During compilation, we will walk the textures (non imported) to create resource cache
		// and bindless table entries for them
		bool isDepthFormat = IsDepthFormat(spec.Format);
		m_Graph.m_Textures.emplace_back(FGTextureEntry{
			.Name = name,
			.Desc = {
				.Extent = ResolveSize(spec.Size, m_Graph.m_OutputExtent),
				.Format = spec.Format,
				.Usage = {},	// Will be built out during compilation by walking the texture accesses for each pass
				.MipLevels = 1
			},
			.Imported = false,
			.InitialState = initialState,
			.FinalState = initialState	// Will be updated during compilation to the last pass's final state
			});
		return { static_cast<uint32_t>(m_Graph.m_Textures.size() - 1) };
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