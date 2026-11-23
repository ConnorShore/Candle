#pragma once

#include <cstdint>

namespace Candle {

	template<typename Tag>
	struct FGResourceHandle
	{
		uint32_t Index{ std::numeric_limits<uint32_t>::max() };	// Into this frame's entries; meaningless after Reset
		uint32_t Version{ 0 };	// Incremented every time a resource is created, so handles to old resources can be detected

		inline bool IsValid() const { return Index != std::numeric_limits<uint32_t>::max(); }
		inline bool IsSameResource(const FGResourceHandle& other) const { return Index == other.Index; }
		bool operator==(const FGResourceHandle& other) const { return IsSameResource(other) && Version == other.Version; }
	};

	struct FGTextureTag {};
	using FGTextureHandle = FGResourceHandle<FGTextureTag>;

	struct FGBufferTag {};
	using FGBufferHandle = FGResourceHandle<FGBufferTag>;

}