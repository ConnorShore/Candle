#pragma once

#include "Candle/Core/Threading/ThreadPriority.h"

#include <chrono>
#include <cstdint>
#include <thread>

namespace Candle {

	struct Tick
	{
		uint64_t Value = 0;

		inline explicit operator uint64_t() const { return Value; }
	};

	struct CPUTopology
	{
		uint32_t NumPhysicalCores = 0;
		uint32_t NumLogicalCores = 0;
	};

	class Platform
	{
	public:
		static void Init();

		// Time //
		static Tick GetStartTick();
		static Tick GetTick();

		inline static uint64_t ToMicroseconds(const Tick& start, const Tick& end)
		{
			return TicksToMicroseconds(end.Value - start.Value);
		}

		inline static double ToSeconds(const Tick& start, const Tick& end)
		{
			return static_cast<double>(end.Value - start.Value) / s_TickFrequency;
		}

		inline static double ToMilliseconds(const Tick& start, const Tick& end)
		{
			return static_cast<double>(end.Value - start.Value) * 1'000.0 / s_TickFrequency;
		}

		inline static uint64_t ToUnixMicroseconds(const Tick& tick)
		{
			return s_StartUnixMicroseconds + TicksToMicroseconds(tick.Value - s_StartTime);
		}

		// Convenience for formatting, e.g. std::format("{:%H:%M:%S}", Platform::ToSystemClock(t)).
		inline static std::chrono::system_clock::time_point ToSystemClock(const Tick& tick)
		{
			return std::chrono::system_clock::time_point{
				std::chrono::microseconds{ ToUnixMicroseconds(tick) } };
		}

		// Threads //
		static uint32_t GetCurrentThreadId();
		static void SetCurrentThreadName(const char* name);
		static void SetCurrentThreadPriority(ThreadPriority priority);
		static void SetCurrentThreadAffinityMask(uint64_t mask);
		static void SleepCurrentThread(uint32_t milliseconds);
		static CPUTopology QueryCPUTopology();
		static uint32_t GetCurrentThreadProcessor();
		static uint32_t GetThreadId(std::thread::native_handle_type handle);

		// Console //
		static bool EnableConsoleAnsiColors();
		static bool DisableConsoleAnsiColors();

	private:
		// Defined per platform alongside the tick queries
		static void InitTime();

		// Split into whole seconds plus remainder so the multiply cannot overflow
		inline static uint64_t TicksToMicroseconds(uint64_t ticks)
		{
			const uint64_t whole = (ticks / s_TickFrequency) * 1'000'000ULL;
			const uint64_t part = (ticks % s_TickFrequency) * 1'000'000ULL / s_TickFrequency;
			return whole + part;
		}

	private:
		inline static uint64_t s_TickFrequency = 0;
		inline static uint64_t s_StartTime = 0;
		inline static uint64_t s_StartUnixMicroseconds = 0;
	};

}
