#pragma once

#include <cstdint>

namespace Candle {

	template<typename Tag>
	struct FGResourceHandle
	{
		uint32_t Index{ std::numeric_limits<uint32_t>::max() };	// Into this frame's entries; meaningless after Reset

		inline bool IsValid() const { return Index != std::numeric_limits<uint32_t>::max(); }
		bool operator==(const FGResourceHandle&) const = default;
	};

	struct FGTextureTag {};
	using FGTextureHandle = FGResourceHandle<FGTextureTag>;

	struct FGBufferTag {};
	using FGBufferHandle = FGResourceHandle<FGBufferTag>;

}