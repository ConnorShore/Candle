#include "cdlpch.h"
#include "ResourceCache.h"
#include "FrameGraphResources.h"

#include "Candle/Render/Vulkan/GPUAllocator.h"
#include "Candle/Render/Vulkan/BindlessTable.h"

namespace Candle {

	ResourceCache::ResourceCache(GPUAllocator& allocator, BindlessTable& bindlessTable)
		: m_Allocator(allocator), m_BindlessTable(bindlessTable)
	{
	}

	ResourceCache::~ResourceCache()
	{

	}

	CacheImportResult ResourceCache::CreateOrGetTexture(FGTextureKey key, uint64_t frameIndex)
	{
		// Check if a texture with the same key already exists in the cache (and hasn't been used in this frame yet). If so, return it.
		for (size_t i = 0; i < m_Cache.size(); ++i)
		{
			if (m_Cache[i].SlotState != CacheSlotState::Free && m_Cache[i].Key == key && m_Cache[i].LastFrameUsed < frameIndex)
			{
				m_Cache[i].LastFrameUsed = frameIndex; // Update the last frame used,
				m_Cache[i].SlotState = CacheSlotState::Live;	// A retiring entry's image and descriptor are untouched, so it can come straight back
				return CacheImportResult{
					.CacheIndex = static_cast<uint32_t>(i),
					.BindlessIndex = m_Cache[i].BindlessIndex,
					.Image = m_Cache[i].Image.GetImage(),
					.ImageView = m_Cache[i].Image.GetImageView(),
					.State = m_Cache[i].State
				};
			}
		}

		CacheIndex slot;
		if (!m_FreeSlots.empty())
		{
			slot = m_FreeSlots.back();
			m_FreeSlots.pop_back();
		}
		else
		{
			slot = static_cast<CacheIndex>(m_Cache.size());
			m_Cache.emplace_back();
		}

		// Create the image and write it to the bindless table
		GPUImage image = m_Allocator.CreateImage({
			.Extent{ key.Extent.width, key.Extent.height },
			.Format{ key.Format },
			.Usage{ key.Usage },
			.Aspect{ GetAspectMask(key.Format) },
			.Flags{ VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT },
			.DebugName{ std::format("FrameGraph Transient Texture {}", slot) }
		});

		// Write the image to the bindless table and get its index
		uint32_t outIndex = std::numeric_limits<uint32_t>::max();	// Invalid to start
		if (key.Usage & vk::ImageUsageFlagBits::eSampled)
			outIndex = m_BindlessTable.WriteFrameGraphTexture(slot, image.GetImageView());
		if (key.Usage & vk::ImageUsageFlagBits::eStorage)
			outIndex = m_BindlessTable.WriteFrameGraphStorageImage(slot, image.GetImageView());

		// Create a new texture in the resource cache
		CacheEntry& newEntry = m_Cache[slot];
		newEntry = CacheEntry{
			.Image{ std::move(image) },
			.Key{ key },
			.State{				// Assume the image is fresh and has never been used, so it is in an undefined state
				.Layout = vk::ImageLayout::eUndefined,
				.Access = vk::AccessFlagBits2::eNone,
				.Stage = vk::PipelineStageFlagBits2::eNone
			},
			.BindlessIndex{ outIndex },
			.LastFrameUsed{ frameIndex },
			.RetireValue{ 0 },
			.SlotState{ CacheSlotState::Live }
		};

		return CacheImportResult{
			.CacheIndex = slot,
			.BindlessIndex = newEntry.BindlessIndex,
			.Image = newEntry.Image.GetImage(),
			.ImageView = newEntry.Image.GetImageView(),
			.State = newEntry.State
		};
	}

	void ResourceCache::SetTextureState(CacheIndex index, const ImageState& state)
	{
		CDL_CORE_ASSERT(index < m_Cache.size(), "CacheIndex is out of range");
		m_Cache[index].State = state;
	}

	void ResourceCache::EvictUnusedEntries(uint64_t frameIndex, uint64_t lastSubmittedValue, uint64_t completedValue)
	{
		for (CacheIndex i = 0; i < m_Cache.size(); ++i)
		{
			CacheEntry& entry = m_Cache[i];

			// Every frame that used it has already been submitted, so the last submit covers all of them
			if (entry.SlotState == CacheSlotState::Live && entry.LastFrameUsed + kMaxUnusedFramesBeforeEviction < frameIndex)
			{
				entry.SlotState = CacheSlotState::Retiring;
				entry.RetireValue = lastSubmittedValue;
			}

			// The slot waits with the image, since its bindless element can't be rewritten while a pending frame may read it
			if (entry.SlotState == CacheSlotState::Retiring && completedValue >= entry.RetireValue)
			{
				entry.Image = GPUImage{};
				entry.SlotState = CacheSlotState::Free;
				m_FreeSlots.push_back(i);
			}
		}
	}

}