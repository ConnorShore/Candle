#include "cdlpch.h"
#include "FrameGraph.h"
#include "FrameGraphResources.h"

#include "Candle/Render/Vulkan/RenderDevice.h"
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

		constexpr vk::AccessFlags2 kWriteAccesses = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderStorageWrite
			| vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
			| vk::AccessFlagBits2::eTransferWrite | vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eMemoryWrite;

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

	FrameGraph::FrameGraph(FrameGraphSpec spec)
		: m_TexturePool(spec.TexturePool)
		, m_PipelineStore(spec.PipelineStore)
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
		m_Accesses.clear();
		m_CompiledGraph.Reset();
		m_Blackboard.Clear();

		m_ResetBeforeCompile = true;
	}

	void FrameGraph::CullPasses()
	{
		CDL_PROFILE_SCOPE("FrameGraph CullPasses");

		std::vector<uint32_t> livePasses;
		livePasses.reserve(m_Passes.size());

		// Walk the passes and mark any that are NeverCull or write to an imported texture as live. The rest will be culled.
		for (size_t i = 0; i < m_Passes.size(); ++i)
		{
			FrameGraphPassNode& passNode = m_Passes[i];
			bool isLastImportWrite = std::any_of(m_Textures.begin(), m_Textures.end(), [i](const FGTextureEntry& texture) {
				return texture.LastWritePassIndex == i && texture.Imported;
				});
			if (passNode.NeverCull || isLastImportWrite)
			{
				livePasses.push_back(static_cast<uint32_t>(i));
				continue;
			}
		}

		// Walk the live passes and mark any passes that wrote to the textures they read from as live, since they are needed to produce the inputs for the live passes
		while (!livePasses.empty())
		{
			uint32_t passIndex = livePasses.back();
			livePasses.pop_back();

			FrameGraphPassNode& passNode = m_Passes[passIndex];
			if (passNode.IsLive)
				continue;	// Already marked as live, no need to process again

			passNode.IsLive = true;

			// Walk the accesses for this pass and mark any passes that wrote to those resources as live, since they are needed to produce the inputs for this pass
			for (const FGAccessEntry& access : GetPassAccesses(passIndex))
			{
				uint32_t srcPassIndex = access.SrcPassIndex;
				if (srcPassIndex < m_Passes.size() && !m_Passes[srcPassIndex].IsLive)
					livePasses.push_back(srcPassIndex);
			}
		}
	}

	void FrameGraph::Compile(uint64_t frameIndex)
	{
		CDL_PROFILE_SCOPE("FrameGraph Compile");

		CDL_CORE_ASSERT(m_ResetBeforeCompile, "FrameGraph must be reset before compiling");
		m_ResetBeforeCompile = false;	// Reset the flag for next frame

		// Cull passes that don't write to imported textures and aren't marked as NeverCull
		CullPasses();

		// Walk attachments and OR together usages for each attachment to determine the final usage for the resource cache and bindless table
		for (uint32_t pass = 0; pass < m_Passes.size(); ++pass)
		{
			if (!m_Passes[pass].IsLive)
				continue;

			for (const FGAccessEntry& textureAccess : GetPassAccesses(pass))
			{
				if (textureAccess.Type != FGResourceType::Texture)
					continue;

				FGTextureEntry& texture = m_Textures[textureAccess.ResourceIndex];
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
			if (!texture.Desc.Usage)
				continue;	// Only live passes add usage, so none uses it. A culled last writer isn't enough, earlier writers may be live.

			FGTextureKey key{
				.Format{ texture.Desc.Format },
				.Extent{ vk::Extent2D(texture.Desc.Extent.x, texture.Desc.Extent.y) },
				.Usage{ texture.Desc.Usage }
			};
			CacheImportResult cacheResult = m_ResourceCache.CreateOrGetTexture(key, frameIndex);

			// Assign the cache and bindless table entries to the texture
			texture.Image = cacheResult.Image;
			texture.ImageView = cacheResult.ImageView;
			texture.BindlessIndex = cacheResult.BindlessIndex;
			texture.CacheIndex = cacheResult.CacheIndex;

			// Earlier frames in flight share this image, so the first barrier waits for their access. The layout is dropped:
			// transient contents don't carry across frames, and Undefined stays correct even if the cached layout is stale.
			texture.InitialState = {
				.Layout = vk::ImageLayout::eUndefined,
				.Access = cacheResult.State.Access,
				.Stage = cacheResult.State.Stage
			};
			texture.FinalState = texture.InitialState;
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
		for (uint32_t pass = 0; pass < m_Passes.size(); ++pass)
		{
			if (!m_Passes[pass].IsLive)
				continue;

			FrameGraphPassNode& passNode = m_Passes[pass];
			uint32_t barrierStartIndex = static_cast<uint32_t>(m_CompiledGraph.Barriers.size());
			uint32_t attachmentStartIndex = static_cast<uint32_t>(m_CompiledGraph.Attachments.size());
			int depthAttachmentIndex = -1;

			// Walk this pass's texture accesses, creating a barrier and compiled attachment for each if needed
			const std::span<const FGAccessEntry> accesses = GetPassAccesses(pass);
			for (size_t access = 0; access < accesses.size(); ++access)
			{
				const FGAccessEntry& textureAccess = accesses[access];
				if (textureAccess.Type != FGResourceType::Texture)
					continue;

				uint32_t textureIndex = textureAccess.ResourceIndex;
				FGTextureEntry& texture = m_Textures[textureIndex];
				FGUsageInfo usageInfo = GetUsageInfo(textureAccess.Usage);
				if (usageInfo.ImageUsage & vk::ImageUsageFlagBits::eSampled)
					usageInfo.State.Layout = texture.SampledLayout;	// The layout its bindless descriptor was written with

				// Create a barrier for this texture access if necessary (can skip if it's a read, the texture is already in the correct layout and nothing has written it since its last barrier)
				bool canSkipBarrier = (!textureAccess.Write && !(texture.FinalState.Access & kWriteAccesses) && texture.FinalState.Layout == usageInfo.State.Layout);
				if (canSkipBarrier)
				{
					// If the texture is already in the correct layout for a read, we can skip creating a barrier, 
					// but we still need to update the final state to include the new access flags and stage flags
					texture.FinalState.Stage |= usageInfo.State.Stage;	
					texture.FinalState.Access |= usageInfo.State.Access;
					texture.FinalState.Layout = usageInfo.State.Layout;
				}
				else
				{
					FGBarrier barrier{
						.Texture{ textureAccess.GetTextureHandle() },
						.FromState{ texture.FinalState },
						.ToState{ usageInfo.State }
					};

					m_CompiledGraph.Barriers.push_back(barrier);

					// Update the texture's final state for the next pass
					m_Textures[textureIndex].FinalState = usageInfo.State;
				}

				// If this is a color or depth attachment, create a compiled attachment
				if (textureAccess.Usage == FGUsage::ColorAttachment || textureAccess.Usage == FGUsage::DepthAttachment)
				{
					if (textureAccess.Usage == FGUsage::DepthAttachment)
					{
						depthAttachmentIndex = static_cast<int>(access);
						continue;
					}
					
					FGCompiledAttachment compiledAttachment{
						.Texture{ textureAccess.GetTextureHandle() },
						.Layout{ usageInfo.State.Layout },
						.LoadOp{ ToAttachmentLoadOp(textureAccess.LoadOp) },
						.StoreOp{ vk::AttachmentStoreOp::eStore }, // TODO: Make this configurable
						.ClearValue{ textureAccess.ClearValue }
					};
					m_CompiledGraph.Attachments.push_back(compiledAttachment);
				}
			}

			// If there was a depth attachment, create its compiled attachment after the color attachments
			if (depthAttachmentIndex >= 0)
			{
				const FGAccessEntry& depthAccess = accesses[depthAttachmentIndex];
				FGUsageInfo depthUsageInfo = GetUsageInfo(depthAccess.Usage);
				FGCompiledAttachment depthAttachment{
					.Texture{ depthAccess.GetTextureHandle() },
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
				.PassIndex = static_cast<uint32_t>(pass),
				.BarrierStartIndex = barrierStartIndex,
				.BarrierCount = barrierCount,
				.AttachmentStartIndex = attachmentStartIndex,
				.AttachmentCount = attachmentCount,
				.HasDepthAttachment = depthAttachmentIndex >= 0,
				.IsComputePass = passNode.IsComputePass
			});
		}

		// Walk the final barriers and update their from states to the latest state of the texture at the end of the frame
		for (FGBarrier& finalBarrier : m_CompiledGraph.FinalBarriers)
		{
			FGTextureEntry& texture = m_Textures[finalBarrier.Texture.Index];
			finalBarrier.FromState = texture.FinalState;
			texture.FinalState = finalBarrier.ToState;	// Update the texture's final state for the next frame
		}

		// Next frame's first barrier waits on this. Everything runs on the graphics queue, so a barrier there
		// also covers last frame's command buffer; a second queue would need a semaphore instead.
		for (const FGTextureEntry& texture : m_Textures)
		{
			if (!texture.Imported && texture.Desc.Usage)
				m_ResourceCache.SetTextureState(texture.CacheIndex, texture.FinalState);
		}
	}

	void FrameGraph::EvictUnusedResources(uint64_t frameIndex, uint64_t lastSubmittedValue, uint64_t completedValue)
	{
		CDL_CORE_ASSERT(!m_ResetBeforeCompile, "FrameGraph must be compiled before evicting, or this frame's transients look unused");
		m_ResourceCache.EvictUnusedEntries(frameIndex, lastSubmittedValue, completedValue);
	}

	void FrameGraph::Execute(const RenderDevice& renderDevice, vk::raii::CommandBuffer& commandBuffer)
	{
		CDL_PROFILE_SCOPE("FrameGraph Execute");
		// Walk the compiled graph and execute each pass, creating barriers and attachments as needed
		for (const FGCompiledPass& pass : m_CompiledGraph.Passes)
		{
			// Create barriers
			if (pass.BarrierCount > 0)
			{
				std::vector<vk::ImageMemoryBarrier2> barriers;
				barriers.reserve(pass.BarrierCount);
				for (uint32_t i = 0; i < pass.BarrierCount; ++i)
				{
					uint32_t barrierIndex = pass.BarrierStartIndex + i;
					const FGBarrier& barrier = m_CompiledGraph.Barriers[barrierIndex];
					const FGTextureEntry& texture = m_Textures[barrier.Texture.Index];

					barriers.push_back(MakeImageBarrier(
						texture.Image,
						barrier.FromState,
						barrier.ToState,
						{
							.aspectMask = IsDepthFormat(texture.Desc.Format) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
							.levelCount = 1,
							.layerCount = 1
						}	// TODO: Handle and support subresources (CAN-37)
					));
				}

				commandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = pass.BarrierCount, .pImageMemoryBarriers = barriers.data() });
			}
			
			// Create attachments
			vk::RenderingAttachmentInfo depthAttachment;
			std::vector<vk::RenderingAttachmentInfo> colorAttachments;
			colorAttachments.reserve(pass.AttachmentCount - (pass.HasDepthAttachment ? 1 : 0));
			vk::Extent2D renderExtent{ 0, 0 };

			for (uint32_t i = 0; i < pass.AttachmentCount; ++i)
			{
				uint32_t attachmentIndex = pass.AttachmentStartIndex + i;
				const FGCompiledAttachment& compiledAttachment = m_CompiledGraph.Attachments[attachmentIndex];
				const FGTextureEntry& texture = m_Textures[compiledAttachment.Texture.Index];

				const vk::Extent2D extent{ texture.Desc.Extent.x, texture.Desc.Extent.y };
				CDL_CORE_ASSERT(i == 0 || extent == renderExtent, "A pass's attachments share one render area, so they must all be the same size");
				renderExtent = extent;

				if (pass.HasDepthAttachment && i == pass.AttachmentCount - 1)
				{
					// Create the depth attachment
					depthAttachment = vk::RenderingAttachmentInfo{
						.imageView = texture.ImageView,
						.imageLayout = m_CompiledGraph.Attachments[attachmentIndex].Layout,
						.loadOp = compiledAttachment.LoadOp,
						.storeOp = compiledAttachment.StoreOp,
						.clearValue = compiledAttachment.ClearValue
					};
				}
				else
				{
					// Create the color attachment
					colorAttachments.push_back(vk::RenderingAttachmentInfo{
						.imageView = texture.ImageView,
						.imageLayout = compiledAttachment.Layout,
						.loadOp = compiledAttachment.LoadOp,
						.storeOp = compiledAttachment.StoreOp,
						.clearValue = compiledAttachment.ClearValue
					});
				}
			}

			// Create the rendering info for the render pass
			vk::RenderingInfo renderingInfo{
				.renderArea = { 
					.offset = { 0, 0 }, 
					.extent = renderExtent
				},
				.layerCount = 1,
				.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
				.pColorAttachments = colorAttachments.data(),
				.pDepthAttachment = pass.HasDepthAttachment ? &depthAttachment : nullptr
			};

			// Begin rendering
			GPUDebugScope passDebugInfo(renderDevice, commandBuffer, std::string(pass.Name).c_str());

			// TODO: Eventually remove this and use the pipeline type to determine if it's a compute pass
			if (!pass.IsComputePass)
			{
				commandBuffer.beginRendering(renderingInfo);

				// Set viewport, scissor, depth test
				commandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(renderExtent.height), static_cast<float>(renderExtent.width), -static_cast<float>(renderExtent.height), 0.0f, 1.0f));
				commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D{ 0, 0 }, renderExtent));
			}

			// Execute pass' exec func 
			// (pass is responsible for binding a pipeline, descriptor sets, and issuing draw/dispatch commands)
			FrameGraphPassNode& passNode = m_Passes[pass.PassIndex];
			FrameGraphContext passContext(*this, m_PipelineStore, commandBuffer, pass.PassIndex);
			(*passNode.Pass)(passContext);	// Execute the pass' execute function with the pass context

			// End rendering
			if (!pass.IsComputePass)
				commandBuffer.endRendering();
		}

		// Perform final barriers for imported images
		if (!m_CompiledGraph.FinalBarriers.empty())
		{
			std::vector<vk::ImageMemoryBarrier2> finalBarriers;
			finalBarriers.resize(m_CompiledGraph.FinalBarriers.size());
			for (size_t i = 0; i < m_CompiledGraph.FinalBarriers.size(); ++i)
			{
				const FGBarrier& barrier = m_CompiledGraph.FinalBarriers[i];
				const FGTextureEntry& texture = m_Textures[barrier.Texture.Index];
				finalBarriers[i] = MakeImageBarrier(
					texture.Image,
					barrier.FromState,
					barrier.ToState,
					{
						.aspectMask = IsDepthFormat(texture.Desc.Format) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
						.levelCount = 1,
						.layerCount = 1
					}
				);
			}

			commandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = static_cast<uint32_t>(finalBarriers.size()), .pImageMemoryBarriers = finalBarriers.data() });
		}
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
		return { .Index = static_cast<uint32_t>(m_Textures.size() - 1), .Version = 0 };
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
			.SampledLayout = info.SampledLayout,
			// Earlier reads only have to finish before a write, so no access; later reads have to see the writes
			.InitialState = { .Layout = info.SampledLayout, .Access = vk::AccessFlagBits2::eNone, .Stage = kSampledStages },
			.FinalState = { .Layout = info.SampledLayout, .Access = vk::AccessFlagBits2::eShaderSampledRead, .Stage = kSampledStages }
		});
		return { .Index = static_cast<uint32_t>(m_Textures.size() - 1), .Version = 0 };
	}

}
