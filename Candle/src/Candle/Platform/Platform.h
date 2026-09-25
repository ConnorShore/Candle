#pragma once

#include <chrono>
#include <cstdint>

namespace Candle {

	struct Tick
	{
		uint64_t Value = 0;
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
