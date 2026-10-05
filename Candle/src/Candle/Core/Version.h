#pragma once

#include <cstdint>

namespace Candle {

	constexpr uint8_t CANDLE_VERSION_MAJOR = 0;
	constexpr uint8_t CANDLE_VERSION_MINOR = 1;
	constexpr uint8_t CANDLE_VERSION_PATCH = 0;
	constexpr uint32_t CANDLE_VERSION = (CANDLE_VERSION_MAJOR << 16) | (CANDLE_VERSION_MINOR << 8) | CANDLE_VERSION_PATCH;
}
