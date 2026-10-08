#pragma once

#include "PlatformEvents.h"

#include "Candle/Core/Threading/ThreadPriority.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <string>
#include <vector>
#include <filesystem>
#include <expected>
#include <span>

namespace vk::raii
{
	class PhysicalDevice;
	class SurfaceKHR;
}

namespace Candle {

	class Window;
	class RenderInstance;

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

	// Coarse enough to branch on: a missing asset falls back to a placeholder, a sharing violation is retried.
	enum class FileErrorCode : uint8_t
	{
		NotFound,
		AccessDenied,
		SharingViolation,	// Another handle has the file open incompatibly, e.g. a shader compiler mid-write
		TooLarge,			// More than one read or write call can move (4 GiB - 1 on Windows); OSError is 0
		IO,					// Anything else; FileError::OSError says what
	};

	struct FileError
	{
		FileErrorCode Code = FileErrorCode::IO;
		uint32_t OSError = 0;	// The platform's own code (GetLastError on Windows), for the log
	};

	constexpr const char* ToString(FileErrorCode code)
	{
		switch (code)
		{
		case FileErrorCode::NotFound:			return "NotFound";
		case FileErrorCode::AccessDenied:		return "AccessDenied";
		case FileErrorCode::SharingViolation:	return "SharingViolation";
		case FileErrorCode::TooLarge:			return "TooLarge";
		case FileErrorCode::IO:					return "IO";
		}
		return "Unknown";
	}

	class Platform
	{
	public:
		// OS services only (main-thread id, clocks, etc)
		static void Init();
		static void Shutdown();

		// Window //
		static void InitWindowing();		// Also loads the Vulkan loader, so a machine without one fails here
		static void ShutdownWindowing();
		static bool IsWindowingInitialized();

		// Rendering //
		static const char* const* GetVulkanRequiredInstanceExtensions(uint32_t& outCount);
		static ::vk::raii::SurfaceKHR CreateVulkanSurface(Window& window, RenderInstance& renderInstance);
		static bool GetVulkanPresentationSupport(RenderInstance& renderInstance, const ::vk::raii::PhysicalDevice& physicalDevice, uint32_t queueFamily);

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
		inline static bool IsMainThread() { return GetCurrentThreadId() == s_MainThreadId; }
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
		// Clears outEvents, then appends this frame's events in arrival order. Main thread only, after InitWindowing.
		static void PumpEvents(std::vector<PlatformEvent>& outEvents);

		// Filesystem //
		// Threading: any thread, concurrently; no shared state. Blocking, so never on the main or render thread mid-frame
		static std::expected<std::vector<std::byte>, FileError> ReadFile(const std::filesystem::path& path);
		static std::expected<std::string, FileError> ReadTextFile(const std::filesystem::path& path);
		static std::expected<void, FileError> WriteFile(const std::filesystem::path& path, std::span<const std::byte> data);

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
