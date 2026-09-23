#pragma once

#ifndef NOMINMAX
	#define NOMINMAX // Kills the Windows min/max macros globally for ryml
#endif

#include <iostream>
#include <print>
#include <string>
#include <vector>
#include <array>
#include <queue>
#include <memory>
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <concepts>
#include <format>
#include <fstream>
#include <sstream>
#include <tuple>
#include <optional>
#include <variant>
#include <chrono>
#include <utility>
#include <type_traits>
#include <filesystem>

#ifdef CDL_PLATFORM_WINDOWS
	#include <Windows.h>
#endif

#include "Candle/Core/Core.h"