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

#include "Candle/Memory/Scoped.h"
#include "Candle/Memory/Shared.h"