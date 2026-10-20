#pragma once

// Compiled as both C++ and Slang: slangc gets -I Candle/src, so shaders include interop headers by the same path C++ does.
// slangc's -fvk-use-c-layout gives shader data C++'s layout rules, so one definition is one layout in both languages.
// Interop structs hold data only: no member functions, no bool (4 bytes in Slang, 1 in C++), and no matrices until
// the row/column-major convention between glm and Slang is decided.

#ifdef __cplusplus

#include <glm/glm.hpp>

#include <cstdint>

namespace Candle::GPU {

	using uint = std::uint32_t;
	using float2 = glm::vec2;
	using float3 = glm::vec3;
	using float4 = glm::vec4;
	using int2 = glm::ivec2;
	using int3 = glm::ivec3;
	using int4 = glm::ivec4;
	using uint2 = glm::uvec2;
	using uint3 = glm::uvec3;
	using uint4 = glm::uvec4;

}

#define CDL_INTEROP_BEGIN namespace Candle::GPU {
#define CDL_INTEROP_END }
#define CDL_GPU_PTR(T) std::uint64_t	// A vk::DeviceAddress on this side; the shader reads it as a T*

#else	// Slang

#define CDL_INTEROP_BEGIN
#define CDL_INTEROP_END
#define CDL_GPU_PTR(T) T*

#endif
