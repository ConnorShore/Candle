#pragma once

#include "PlatformEvents.h"

#include "Candle/Core/Threading/ThreadPriority.h"
#include "Candle/Core/WindowSpecification.h"

#include <chrono>
#include <cstdint>
#include <thread>
#include <string>
#include <vector>

namespace Candle {

	using NativeWindowHandle = uintptr_t;

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
		static void Shutdown();

		// Window //
		static NativeWindowHandle CreateWindow(const WindowSpecification& info);
		static void DestroyWindow(NativeWindowHandle handle);
		static void ResizeWindow(NativeWindowHandle handle, uint32_t width, uint32_t height);
		static void MinimizeWindow(NativeWindowHandle handle);
		static void MaximizeWindow(NativeWindowHandle handle);
		static void RestoreWindow(NativeWindowHandle handle);

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
		static void YieldCurrentThread();
		static CPUTopology QueryCPUTopology();
		static uint32_t GetCurrentThreadProcessor();
		static uint32_t GetThreadId(std::thread::native_handle_type handle);

		// Console //
		static bool EnableConsoleAnsiColors();
		static bool DisableConsoleAnsiColors();

		// Events //
		// Pump Events runs on main thread ONLY, and clears the events at the start
		static void PumpEvents(std::vector<PlatformEvent>& outEvents);

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
		inline static uint32_t s_MainThreadId = 0;
	};

}
