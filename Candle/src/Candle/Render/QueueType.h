#pragma once

#include <cstdint>

namespace Candle {

	enum class QueueType : uint8_t
	{
		Graphics,
		Transfer,
		Count
	};

}