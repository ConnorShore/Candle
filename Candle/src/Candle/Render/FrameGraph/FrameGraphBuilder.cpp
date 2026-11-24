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
			.FinalState = initialState,	// Will be updated during compilation to the last pass's final state
			.LastWritePassIndex = m_PassIndex
			});
		return { .Index = static_cast<uint32_t>(m_Graph.m_Textures.size() - 1), .Version = 0 };
	}

	void FrameGraphBuilder::ReadTexture(FGTextureHandle texture, FGUsage usage)
	{
		FGTextureEntry& textureEntry = m_Graph.m_Textures[texture.Index];
		CDL_CORE_ASSERT(textureEntry.Imported || texture.Version > 0, "Cannot read from a texture that has not been written to");
		CDL_CORE_ASSERT(texture.Version == textureEntry.LatestVersion, "Reading from a stale handle");
		CDL_CORE_ASSERT(!GetUsageInfo(usage).Write, "ReadTexture was given a write usage; use WriteTexture");

		// Add an entry to the pass's texture access list, which the graph will use to insert barriers and track usage
		FGAccessEntry accessEntry{
			.Type = FGResourceType::Texture,
			.ResourceIndex = texture.Index,
			.Version = texture.Version,
			.Usage = usage,
			.Write = false,
			.SrcPassIndex = textureEntry.LastWritePassIndex
		};
		m_Graph.m_Accesses.push_back(accessEntry);
	}

	FGTextureHandle FrameGraphBuilder::WriteTexture(FGTextureHandle texture, FGUsage usage)
	{
		CDL_CORE_ASSERT(texture.Index < m_Graph.m_Textures.size(), "FGTextureHandle is not from this frame");
		CDL_CORE_ASSERT(texture.Version == m_Graph.m_Textures[texture.Index].LatestVersion, "Writing to a stale texture handle");
		CDL_CORE_ASSERT(GetUsageInfo(usage).Write, "WriteTexture was given a read-only usage; use ReadTexture");

		// Add an entry to the pass's texture access list, which the graph will use to insert barriers and track usage
		FGAccessEntry accessEntry{
			.Type = FGResourceType::Texture,
			.ResourceIndex = texture.Index,
			.Version = texture.Version,
			.Usage = usage,
			.Write = true,
			.SrcPassIndex = m_Graph.m_Textures[texture.Index].LastWritePassIndex	// A storage write may read what's there first, and the graph can't tell
		};
		m_Graph.m_Accesses.push_back(accessEntry);
		m_Graph.m_Textures[texture.Index].LastWritePassIndex = m_PassIndex;
		return { .Index = texture.Index, .Version = IncrementHandleVersion(texture) };
	}

	FGTextureHandle FrameGraphBuilder::ColorAttachment(FGTextureHandle texture, FGLoad loadOp, vk::ClearColorValue clearColor)
	{
		CDL_CORE_ASSERT(texture.Version == m_Graph.m_Textures[texture.Index].LatestVersion, "Using a stale texture handle for a color attachment");

		// Add an entry to the pass's texture access list, which the graph will use to insert barriers and track usage
		FGAccessEntry accessEntry{
			.Type = FGResourceType::Texture,
			.ResourceIndex = texture.Index,
			.Version = texture.Version,
			.Usage = FGUsage::ColorAttachment,
			.Write = true,
			.LoadOp = loadOp,
			.ClearValue = clearColor,
			.SrcPassIndex = loadOp != FGLoad::Load ? std::numeric_limits<uint32_t>::max() : m_Graph.m_Textures[texture.Index].LastWritePassIndex
		};
		m_Graph.m_Accesses.push_back(accessEntry);
		m_Graph.m_Textures[texture.Index].LastWritePassIndex = m_PassIndex;
		return { .Index = texture.Index, .Version = IncrementHandleVersion(texture) };
	}

	FGTextureHandle FrameGraphBuilder::DepthAttachment(FGTextureHandle texture, FGLoad loadOp)
	{
		CDL_CORE_ASSERT(texture.Version == m_Graph.m_Textures[texture.Index].LatestVersion, "Using a stale texture handle for a depth attachment");

		FGAccessEntry accessEntry{
			.Type = FGResourceType::Texture,
			.ResourceIndex = texture.Index,
			.Version = texture.Version,
			.Usage = FGUsage::DepthAttachment,
			.Write = true,
			.LoadOp = loadOp,
			.ClearValue = vk::ClearDepthStencilValue(1.0f, 0),	// TODO: Depth clear value should be specified by the pass, but for now just using a default
			.SrcPassIndex = loadOp != FGLoad::Load ? std::numeric_limits<uint32_t>::max() : m_Graph.m_Textures[texture.Index].LastWritePassIndex
		};
		m_Graph.m_Accesses.push_back(accessEntry);
		m_Graph.m_Textures[texture.Index].LastWritePassIndex = m_PassIndex;
		return { .Index = texture.Index, .Version = IncrementHandleVersion(texture) };
	}

	uint32_t FrameGraphBuilder::IncrementHandleVersion(FGTextureHandle handle)
	{
		return m_Graph.m_Textures[handle.Index].LatestVersion = ++handle.Version;
	}

}