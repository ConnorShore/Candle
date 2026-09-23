#pragma once

#include <format>
#include <iostream>
#include <cassert>

// Temporary assertion macro for debugging purposes. This will be replaced with a more robust logging and assertion system in the future.
#define CDL_CORE_ASSERT(x, ...) { if(!(x)) { std::cout << std::format("Assertion Failed: {0}", __VA_ARGS__) << std::endl; assert(false); } }

#define CDL_CORE_STATIC_ASSERT(x, ...) { if(!(x)) { std::cout << std::format("Static Assertion Failed: {0}", __VA_ARGS__) << std::endl; static_assert(false, "Static Assertion Failed"); } }