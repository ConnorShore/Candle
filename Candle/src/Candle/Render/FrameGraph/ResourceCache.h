#pragma once

#include "FrameGraphResources.h"
#include "FrameGraphHandles.h"

#include "Candle/Render/Resources/GPUHandles.h"
#include "Candle/Render/Vulkan/GPUImage.h"

#include <cstdint>
#include <vector>

namespace Candle {

	class GPUAllocator;
	class BindlessTable;

	struct FGTextureKey
	{
		vk::Format Format;
		vk::Extent2D Extent;
		vk::ImageUsageFlags Usage;

		bool operator==(const FGTextureKey& other) const
		{
			return Format == other.Format && Extent == other.Extent && Usage == other.Usage;
		}
	};
	enum class CacheSlotState : uint8_t { Live, Retiring, Free };

	struct CacheEntry
	{
		GPUImage Image;
		FGTextureKey Key;
		ImageState State;	// Where the last frame left it
		uint32_t BindlessIndex;
		uint64_t LastFrameUsed;	// The packet FrameIndex, not a timeline value: uploads advance the timeline too
		uint64_t RetireValue;	// Set when it starts retiring; once the GPU reaches it, the image and slot are freed
		CacheSlotState SlotState;
	};

	struct CacheImportResult
	{
		uint32_t CacheIndex;
		uint32_t BindlessIndex;
		vk::Image Image;
		vk::ImageView ImageView;
		ImageState State;
	};

	using CacheIndex = uint32_t;

	class ResourceCache
	{
	public:
		static constexpr uint32_t kMaxUnusedFramesBeforeEviction = 3;	// TODO: Make this configurable

	public:
		ResourceCache(GPUAllocator& allocator, BindlessTable& bindlessTable);
		~ResourceCache();

		// Retrieves a usable entry with a matching key
		// If one doesn't exist a new entry is created (used for transient resources)
		CacheImportResult CreateOrGetTexture(FGTextureKey key, uint64_t frameIndex);

		void SetTextureState(CacheIndex index, const ImageState& state);

		// Retires entries unused for more than kMaxUnusedFramesBeforeEviction frames, and frees the ones the GPU has passed.
		// Call after this frame's CreateOrGetTexture calls, so nothing this frame uses is retired.
		void EvictUnusedEntries(uint64_t frameIndex, uint64_t lastSubmittedValue, uint64_t completedValue);

	private:
		GPUAllocator& m_Allocator;
		BindlessTable& m_BindlessTable;

		std::vector<CacheEntry> m_Cache;
		std::vector<CacheIndex> m_FreeSlots;
	};

}