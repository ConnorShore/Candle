#pragma once

#include "FrameGraphResources.h"
#include "FrameGraphHandles.h"

#include "Candle/Render/Resources/GPUHandles.h"
#include "Candle/Render/Resources/DeletionQueue.h"

#include <vector>

namespace Candle {

	class GPUAllocator;
	class BindlessTable;

	struct FGTextureKey
	{
		vk::Format Format;
		vk::Extent2D Extent;
		FGUsage Usage;

		bool operator==(const FGTextureKey& other) const
		{
			return Format == other.Format && Extent == other.Extent && Usage == other.Usage;
		}
	};
	struct CacheEntry
	{
		const GPUImage* Image;
		FGTextureKey Key;
		ImageState State;	// Where the last frame left it
		uint32_t BindlessIndex;
		uint64_t LastFrameUsed;
	};

	struct CacheImportResult
	{
		uint32_t Index;
		CacheEntry Entry;
	};

	using CacheIndex = uint32_t;

	class ResourceCache
	{
	public:
		ResourceCache(GPUAllocator& allocator, BindlessTable& bindlessTable);
		~ResourceCache();

		// Retrieves a usable entry with a matching key
		// If one doesn't exist a new entry is created (used for transient resources)
		CacheImportResult CreateOrGetTexture(FGTextureKey key, uint64_t currentTimelineVal);

	private:
		GPUAllocator& m_Allocator;
		BindlessTable& m_BindlessTable;

		std::vector<CacheEntry> m_Cache;
		DeletionQueue m_RetiredImages;
	};

}