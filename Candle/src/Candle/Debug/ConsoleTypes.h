#pragma once

#include "ConsoleArgs.h"

#include <functional>

namespace Candle {

	using ConsoleCommandCallback = std::function<void(ConsoleArgs&& args)>;

}