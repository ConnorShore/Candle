#pragma once

#include <cstdint>
#include <limits>

namespace Candle {

	// Tag is only there to make handles of different kinds distinct types
	template<typename Tag>
	struct GPUHandle
	{
		uint32_t Index{ std::numeric_limits<uint32_t>::max() };
		uint32_t Generation{ 0 };

		inline bool IsValid() const { return Index != std::numeric_limits<uint32_t>::max(); }
	};

	struct MeshTag {};
	using GPUMeshHandle = GPUHandle<MeshTag>;

	struct TextureTag {};
	using GPUTextureHandle = GPUHandle<TextureTag>;

	// A handle with the kind erased, so one list can carry slots of every pool
	struct GPUSlotRef
	{
		uint16_t PoolID;
		uint32_t Index;
		uint32_t Generation;
	};

}
