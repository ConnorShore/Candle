#include "cdlpch.h"
#include "ResourceCache.h"

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

	CacheImportResult ResourceCache::CreateOrGetTexture(FGTextureKey key)
	{
		// Check if a texture with the same key already exists in the cache
		for (size_t i = 0; i < m_Cache.size(); ++i)
		{
			if (m_Cache[i].Key == key)
			{
				// TODO NOW: Figure out how to set this
				//m_Cache[i].LastFrameUsed = currentTimelineVal; // Update the last frame used,
				return CacheImportResult{
					.Index = static_cast<uint32_t>(i),
					.Entry = m_Cache[i]
				};
			}
		}

		// TODO NOW: Create bindless index for the new texture (but maybe will happen elsewhere)


		// Create a new texture in the resource cache
		m_Cache.emplace_back(CacheEntry{
			.Image = nullptr, // Placeholder, actual image creation logic should be implemented
			.Key = key,
			.State = { .Layout = vk::ImageLayout::eUndefined, .Access = vk::AccessFlagBits2::eNone, .Stage = vk::PipelineStageFlagBits2::eNone },
			});

		return CacheImportResult{
			.Index = static_cast<uint32_t>(m_Cache.size() - 1),
			.Entry = m_Cache.back()
		};
	}

}