#pragma once

#ifdef _WIN32
#ifdef _WIN64
#define CDL_PLATFORM_WINDOWS
#else
#error "x86 Builds are not supported!"
#endif
#else
#error "Unknown platform!"
#endif

#if defined(CDL_DEBUG)
#define CDL_ENABLE_ASSERTS
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#define CDL_THREAD_PAUSE() _mm_pause() // Hint to CPU that we are in a spin loop, its more efficient than std::this_thread::yield() on x86 architectures
#else
#include <thread>
#define CDL_THREAD_PAUSE() std::this_thread::yield() // Fallback to yield for non-x86 architectures
#endif

#include "Candle/Core/Asserts.h"

#include "Candle/Core/Memory/Scoped.h"
#include "Candle/Core/Memory/Shared.h"
