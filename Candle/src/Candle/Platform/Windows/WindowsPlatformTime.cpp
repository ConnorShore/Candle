#include "cdlpch.h"
#include "Candle/Platform/Platform.h"

#ifndef CDL_PLATFORM_WINDOWS
	#error "WindowsPlatformTime.cpp is Windows-only."
#endif

#include <Windows.h>

namespace Candle {

	// 100ns intervals between the FILETIME epoch (1601-01-01) and the Unix epoch (1970-01-01).
	static constexpr uint64_t s_FiletimeToUnixEpoch = 116'444'736'000'000'000ULL;

	void Platform::InitTime()
	{
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		s_TickFrequency = static_cast<uint64_t>(frequency.QuadPart);

		LARGE_INTEGER startTime;
		QueryPerformanceCounter(&startTime);
		s_StartTime = static_cast<uint64_t>(startTime.QuadPart);

		FILETIME fileTime;
		GetSystemTimePreciseAsFileTime(&fileTime);

		ULARGE_INTEGER wallClock;
		wallClock.LowPart = fileTime.dwLowDateTime;
		wallClock.HighPart = fileTime.dwHighDateTime;
		s_StartUnixMicroseconds = (wallClock.QuadPart - s_FiletimeToUnixEpoch) / 10ULL;	// Convert from 100ns intervals to microseconds
	}

	Tick Platform::GetStartTick()
	{
		return Tick{ s_StartTime };
	}

	Tick Platform::GetTick()
	{
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);
		return Tick{ static_cast<uint64_t>(counter.QuadPart) };
	}

}
