#pragma once

#include <cstdint>
#include <limits>

namespace Candle {

	struct GPUMeshHandle
	{
		uint32_t Index{ std::numeric_limits<uint32_t>::max() };
		uint32_t Generation{ 0 };

		bool IsValid() const { return Index != std::numeric_limits<uint32_t>::max(); }
	};

}