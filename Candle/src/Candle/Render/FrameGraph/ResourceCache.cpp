#include "cdlpch.h"
#include "ResourceCache.h"
#include "FrameGraphResources.h"

#include "Candle/Render/Vulkan/GPUAllocator.h"
#include "Candle/Render/Vulkan/BindlessTable.h"

namespace Candle {

	namespace {

		vk::ImageAspectFlags GetAspectFlags(vk::Format format)
		{
			switch (format)
			{
			case vk::Format::eD16Unorm:
			case vk::Format::eX8D24UnormPack32:
			case vk::Format::eD32Sfloat:
				return vk::ImageAspectFlagBits::eDepth;
			case vk::Format::eD16UnormS8Uint:
			case vk::Format::eD24UnormS8Uint:
			case vk::Format::eD32SfloatS8Uint:
				return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
			default:
				return vk::ImageAspectFlagBits::eColor;
			}
		}

	}

	ResourceCache::ResourceCache(GPUAllocator& allocator, BindlessTable& bindlessTable)
		: m_Allocator(allocator), m_BindlessTable(bindlessTable)
	{
	}

	ResourceCache::~ResourceCache()
	{

	}

	CacheImportResult ResourceCache::CreateOrGetTexture(FGTextureKey key, uint64_t currentTimelineVal)
	{
		// Check if a texture with the same key already exists in the cache
		for (size_t i = 0; i < m_Cache.size(); ++i)
		{
			if (m_Cache[i].Key == key)
			{
				m_Cache[i].LastFrameUsed = currentTimelineVal; // Update the last frame used,
				return CacheImportResult{
					.CacheIndex = static_cast<uint32_t>(i),
					.BindlessIndex = m_Cache[i].BindlessIndex,
					.Image = m_Cache[i].Image.GetImage(),
					.ImageView = m_Cache[i].Image.GetImageView()	
				};
			}
		}

		// TODO NOW: See if race condition can happen with bindless table insert
		uint32_t cacheSize = static_cast<uint32_t>(m_Cache.size());

		// Create the image and write it to the bindless table
		GPUImage image = m_Allocator.CreateImage({
			.Extent{ key.Extent.width, key.Extent.height },
			.Format{ key.Format },
			.Usage{ key.Usage },
			.Aspect{ GetAspectFlags(key.Format) },
			.Flags{ VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT },
			.DebugName{ std::format("FrameGraph Transient Texture {}", m_Cache.size()) }
		});
		m_BindlessTable.WriteFrameGraphTexture(static_cast<uint32_t>(m_Cache.size()), image.GetImageView());

		// Create a new texture in the resource cache
		CacheEntry& newEntry = m_Cache.emplace_back(CacheEntry{
			.Image{ std::move(image) },
			.Key{ key },
			.State{				// Assume the image is fresh and has never been used, so it is in an undefined state
				.Layout = vk::ImageLayout::eUndefined, 
				.Access = vk::AccessFlagBits2::eNone, 
				.Stage = vk::PipelineStageFlagBits2::eNone 
			},
			.BindlessIndex{ cacheSize },
			.LastFrameUsed{ currentTimelineVal }
		});

		return CacheImportResult{
			.CacheIndex = cacheSize,
			.BindlessIndex = newEntry.BindlessIndex,
			.Image = newEntry.Image.GetImage(),
			.ImageView = newEntry.Image.GetImageView()
		};
	}

}