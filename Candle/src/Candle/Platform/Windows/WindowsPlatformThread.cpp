#include "cdlpch.h"
#include "Candle/Core/Asserts.h"
#include "Candle/Platform/Platform.h"

#ifndef CDL_PLATFORM_WINDOWS
#error "WindowsPlatformThread.cpp is Windows-only."
#endif

#include <Windows.h>

namespace Candle {

	uint32_t Platform::GetCurrentThreadId()
	{
		return static_cast<uint32_t>(::GetCurrentThreadId());
	}

	void Platform::SetCurrentThreadName(const char* name)
	{
		SetThreadDescription(::GetCurrentThread(), std::wstring(name, name + strlen(name)).c_str());
	}

	void Platform::SetCurrentThreadPriority(ThreadPriority priority)
	{
		int winPriority = THREAD_PRIORITY_NORMAL;
		switch (priority)
		{
		case ThreadPriority::Low:
			winPriority = THREAD_PRIORITY_BELOW_NORMAL;
			break;
		case ThreadPriority::Normal:
			winPriority = THREAD_PRIORITY_NORMAL;
			break;
		case ThreadPriority::High:
			winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
			break;
		case ThreadPriority::Highest:
			winPriority = THREAD_PRIORITY_HIGHEST;
			break;
		}

		if (!SetThreadPriority(::GetCurrentThread(), winPriority))
			CDL_CORE_ERROR(LogChannel::Thread, "Failed to set thread priority on Windows!");
	}

	void Platform::SetCurrentThreadAffinityMask(uint64_t mask)
	{
		SetThreadAffinityMask(::GetCurrentThread(), static_cast<DWORD_PTR>(mask));
	}

	void Platform::SleepCurrentThread(uint32_t milliseconds)
	{
		Sleep(milliseconds);
	}

	CPUTopology Platform::QueryCPUTopology()
	{
		// The first call fails by design and reports the required buffer size.
		DWORD bytes = 0;
		::GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);

		std::vector<std::byte> buffer(bytes);
		if (!::GetLogicalProcessorInformationEx(RelationProcessorCore,
			reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data()), &bytes))
		{
			const uint32_t logical = std::max(1u, std::thread::hardware_concurrency());
			return CPUTopology{ logical, logical };
		}

		CPUTopology topology;
		// Entries are variable-length, so advance by each entry's Size rather than by sizeof.
		for (DWORD offset = 0; offset < bytes;)
		{
			const auto* entry = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);

			++topology.NumPhysicalCores;
			for (WORD group = 0; group < entry->Processor.GroupCount; ++group)
				topology.NumLogicalCores += static_cast<uint32_t>(std::popcount(entry->Processor.GroupMask[group].Mask));

			offset += entry->Size;
		}
		return topology;
	}

	uint32_t Platform::GetCurrentThreadProcessor()
	{
		return static_cast<uint32_t>(::GetCurrentProcessorNumber());
	}

	uint32_t Platform::GetThreadId(std::thread::native_handle_type handle)
	{
		return static_cast<uint32_t>(::GetThreadId(handle));
	}
}
