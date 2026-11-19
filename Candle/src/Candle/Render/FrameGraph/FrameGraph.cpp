#include "cdlpch.h"
#include "FrameGraph.h"
#include "FrameGraphResources.h"

#include "Candle/Render/Vulkan/GPUAllocator.h"
#include "Candle/Render/Vulkan/BindlessTable.h"
#include "Candle/Render/Vulkan/SwapChainImage.h"
#include "Candle/Render/Resources/TexturePool.h"

namespace Candle {

	namespace {
		constexpr size_t kFrameArenaSize = 1024 * 1024;	// 1 MB for now, may need to be larger if passes have a lot of data

		// Every stage a pool texture may be sampled in outside the graph, the same ones GPUUploader publishes uploads to
		constexpr vk::PipelineStageFlags2 kSampledStages = vk::PipelineStageFlagBits2::eVertexShader
			| vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader;

		// Presentation reads it outside our command stream, so the present semaphore does the waiting, as in OldRender
		constexpr ImageState kPresentState{ .Layout = vk::ImageLayout::ePresentSrcKHR, .Access = vk::AccessFlagBits2::eNone };

		vk::AttachmentLoadOp ToAttachmentLoadOp(FGLoad load)
		{
			switch (load)
			{
			case FGLoad::Load:
				return vk::AttachmentLoadOp::eLoad;
			case FGLoad::Clear:
				return vk::AttachmentLoadOp::eClear;
			case FGLoad::DontCare:
				return vk::AttachmentLoadOp::eDontCare;
			default:
				CDL_CORE_ASSERT(false, "Unknown FGLoad value");
				return vk::AttachmentLoadOp::eDontCare;
			}
		}

	}

	FrameGraph::FrameGraph(FrameGraphSpec spec)
		: m_TexturePool(spec.TexturePool)
		, m_FrameArena(kFrameArenaSize)
		, m_ResourceCache(spec.Allocator, spec.BindlessTable)
	{
	}

	FrameGraph::~FrameGraph()
	{

	}

	void FrameGraph::Reset(glm::uvec2 outputExtent)
	{
		m_OutputExtent = outputExtent;
		m_Passes.clear();
		m_Textures.clear();
		m_FrameArena.Reset();
		m_TextureAccesses.clear();
		m_CompiledGraph.Reset();

		m_ResetBeforeCompile = true;
	}

	void FrameGraph::Compile(uint64_t currentTimelineVal)
	{
		CDL_CORE_ASSERT(m_ResetBeforeCompile, "FrameGraph must be reset before compiling");
		m_ResetBeforeCompile = false;	// Reset the flag for next frame

		// Walk attachments and OR together usages for each attachment to determine the final usage for the resource cache and bindless table
		for (size_t pass = 0; pass < m_TextureAccesses.size(); ++pass)
		{
			for (size_t access = 0; access < m_TextureAccesses[pass].size(); ++access)
			{
				FGTextureAccessEntry& textureAccess = m_TextureAccesses[pass][access];
				FGTextureEntry& texture = m_Textures[textureAccess.Handle.Index];
				FGUsageInfo usageInfo = GetUsageInfo(textureAccess.Usage);

				// OR together the usages for this attachment
				texture.Desc.Usage |= usageInfo.ImageUsage;
			}
		}

		// Walk texture array and for transient (non-imported) textures,
		// create/get a resource cache entry and bindless table entry for it and assign them
		for (size_t i = 0; i < m_Textures.size(); ++i)
		{
			FGTextureEntry& texture = m_Textures[i];
			if (texture.Imported)
				continue;

			FGTextureKey key{
				.Format{ texture.Desc.Format },
				.Extent{ vk::Extent2D(texture.Desc.Extent.x, texture.Desc.Extent.y) },
				.Usage{ texture.Desc.Usage }
			};
			CacheImportResult cacheResult = m_ResourceCache.CreateOrGetTexture(key, currentTimelineVal);

			// Assign the cache and bindless table entries to the texture
			texture.Image = cacheResult.Image;
			texture.ImageView = cacheResult.ImageView;
			texture.BindlessIndex = cacheResult.BindlessIndex;
			texture.CacheIndex = cacheResult.CacheIndex;
		}

		// Walk texture array and for imported textures, create a final barrier to transition them to their final state for the next frame
		for (size_t i = 0; i < m_Textures.size(); ++i)
		{
			FGTextureEntry& texture = m_Textures[i];
			if (texture.Imported)
			{
				FGBarrier barrier{
					.Texture{ { static_cast<uint32_t>(i) } },
					.FromState{ texture.InitialState },		// Will update this from state through compilation so from state is the latest state the texture was in before the end of the frame
					.ToState{ texture.FinalState }
				};
				m_CompiledGraph.FinalBarriers.push_back(barrier);

				// Set texture final state to initial state for the upcoming frame, since the final barrier will transition it to its final state for the next frame
				m_Textures[i].FinalState = m_Textures[i].InitialState;
			}
		}

		// Create the compiled graph by walking the passes and their texture accesses, creating barriers and compiled attachments as needed
		for (size_t pass = 0; pass < m_Passes.size(); ++pass)
		{
			FrameGraphPassNode& passNode = m_Passes[pass];
			uint32_t barrierStartIndex = static_cast<uint32_t>(m_CompiledGraph.Barriers.size());
			uint32_t attachmentStartIndex = static_cast<uint32_t>(m_CompiledGraph.Attachments.size());
			int depthAttachmentIndex = -1;

			// Walk texture entries and for each texture access entry that belongs to this pass, create a barrier and compiled attachment if needed
			for (size_t access = 0; access < m_TextureAccesses[pass].size(); ++access)
			{
				FGTextureAccessEntry& textureAccess = m_TextureAccesses[pass][access];
				FGTextureEntry& texture = m_Textures[textureAccess.Handle.Index];
				FGUsageInfo usageInfo = GetUsageInfo(textureAccess.Usage);

				// Create a barrier for this texture access
				FGBarrier barrier{
					.Texture{ textureAccess.Handle },
					.FromState{ texture.FinalState },
					.ToState{ usageInfo.State }
				};
				m_CompiledGraph.Barriers.push_back(barrier);

				// Update the texture's final state for the next pass
				m_Textures[textureAccess.Handle.Index].FinalState = usageInfo.State;	

				// If this is a color or depth attachment, create a compiled attachment
				if (textureAccess.Usage == FGUsage::ColorAttachment || textureAccess.Usage == FGUsage::DepthAttachment)
				{
					if (textureAccess.Usage == FGUsage::DepthAttachment)
					{
						depthAttachmentIndex = static_cast<int>(access);
						continue;
					}
					
					FGCompiledAttachment compiledAttachment{
						.Texture{ textureAccess.Handle },
						.Layout{ usageInfo.State.Layout },
						.LoadOp{ ToAttachmentLoadOp(textureAccess.LoadOp) },
						.StoreOp{ vk::AttachmentStoreOp::eStore }, // TODO: Make this configurable
						.ClearValue{ textureAccess.ClearVlaue }
					};
					m_CompiledGraph.Attachments.push_back(compiledAttachment);
				}
			}

			// If there was a depth attachment, create its compiled attachment after the color attachments
			if (depthAttachmentIndex >= 0)
			{
				FGTextureAccessEntry& depthAccess = m_TextureAccesses[pass][depthAttachmentIndex];
				FGTextureEntry& depthTexture = m_Textures[depthAccess.Handle.Index];
				FGUsageInfo depthUsageInfo = GetUsageInfo(depthAccess.Usage);
				FGCompiledAttachment depthAttachment{
					.Texture{ depthAccess.Handle },
					.Layout{ depthUsageInfo.State.Layout },
					.LoadOp{ ToAttachmentLoadOp(depthAccess.LoadOp) },
					.StoreOp{ vk::AttachmentStoreOp::eStore }, // TODO: Make this configurable
					.ClearValue{ vk::ClearDepthStencilValue{ 1.0f, 0 } }
				};
				m_CompiledGraph.Attachments.push_back(depthAttachment);
			}

			uint32_t barrierCount = static_cast<uint32_t>(m_CompiledGraph.Barriers.size()) - barrierStartIndex;
			uint32_t attachmentCount = static_cast<uint32_t>(m_CompiledGraph.Attachments.size()) - attachmentStartIndex;
			m_CompiledGraph.Passes.push_back({
				.Name = passNode.Name,
				.BarrierStartIndex = barrierStartIndex,
				.BarrierCount = barrierCount,
				.AttachmentStartIndex = attachmentStartIndex,
				.AttachmentCount = attachmentCount,
				.HasDepthAttachment = depthAttachmentIndex >= 0
			});
		}

		// Walk the final barriers and update their from states to the latest state of the texture at the end of the frame
		for (FGBarrier& finalBarrier : m_CompiledGraph.FinalBarriers)
		{
			FGTextureEntry& texture = m_Textures[finalBarrier.Texture.Index];
			finalBarrier.FromState = texture.FinalState;
		}
	}

	void FrameGraph::Execute(vk::CommandBuffer& commandBuffer)
	{
		// After frame graph is compiled, walk the passes and execute them in order,
		// converting the FG objects into the appropriate Vulkan objects and binding them to the command buffer
	}

	FGTextureHandle FrameGraph::ImportSwapChainImage(SwapChainImage& image)
	{
		m_Textures.push_back({
			.Name = "SwapChain",
			.Desc = image.GetDesc(),
			.Imported = true,
			.Image = image.GetImage(),
			.ImageView = image.GetImageView(),
			.InitialState = image.GetState(),	// Acquire leaves it at the semaphore's wait stage, so the first barrier chains with the wait
			.FinalState = kPresentState
		});
		return { static_cast<uint32_t>(m_Textures.size() - 1) };
	}

	FGTextureHandle FrameGraph::ImportTexture(std::string_view name, GPUTextureHandle texture)
	{
		// The default handle resolves to the error texture, which no pass may write
		CDL_CORE_ASSERT(texture.IsValid(), "ImportTexture was given an invalid texture handle");
		const TextureInfo info = m_TexturePool.GetTextureInfo(texture);

		m_Textures.push_back({
			.Name = name,
			.Desc = info.Desc,
			.Imported = true,
			.Image = info.Image,
			.ImageView = info.ImageView,
			.BindlessIndex = info.Index,
			// Earlier reads only have to finish before a write, so no access; later reads have to see the writes
			.InitialState = { .Layout = info.SampledLayout, .Access = vk::AccessFlagBits2::eNone, .Stage = kSampledStages },
			.FinalState = { .Layout = info.SampledLayout, .Access = vk::AccessFlagBits2::eShaderSampledRead, .Stage = kSampledStages }
		});
		return { static_cast<uint32_t>(m_Textures.size() - 1) };
	}

}
