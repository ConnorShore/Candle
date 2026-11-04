#pragma once

#include "ConsoleArgs.h"

#include <functional>

namespace Candle {

	using ConsoleCommandHandle = uint32_t;
	using ConsoleCommandCallback = std::function<void(ConsoleArgs&& args)>;

	constexpr ConsoleCommandHandle kInvalidCommandHandle = 0;

}

#include "ScopedCommandHandle.h"