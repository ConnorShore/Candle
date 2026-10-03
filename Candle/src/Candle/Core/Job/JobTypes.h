#pragma once

#include "Candle/Core/Memory/Collections/SimpleConcurrentQueue.h"

#include <atomic>
#include <array>
#include <functional>
#include <string>

namespace Candle {

	using EntryFn = void(*)(uintptr_t);

	enum class JobPriority
	{
		Low = 0,
		Normal = 1,
		High = 2,
		Count
	};

	constexpr JobPriority kPriorityOrder[] = {
		JobPriority::High,
		JobPriority::Normal,
		JobPriority::Low
	};


	struct JobSpec
	{
		EntryFn m_EntryFunc{ nullptr };
		uintptr_t m_FuncData{ 0 };
		JobPriority m_Priority{ JobPriority::Normal };
		const char* m_Name{ "Job" };
	};

	// TODO: Once working, see if we can simplify or trim this down to a smaller size
	struct JobRunSlot
	{
		std::atomic<uint32_t> m_JobCount{ 0 };		// Tracks remaining "jobs" remaining in current batch (jobs are done when this is 0)
		std::vector<uint32_t> m_Successors{ };		// Job Slot indicies depending on this run slot finishing (should be notified once this is done)
		std::atomic<uint32_t> m_Remaining{ 0 };		// Tracks remaining successors that need to be completed before we can run the jobs in this slot (successors are done when this is 0)
		std::atomic<uint32_t> m_Generation{ 0 };	// Generation number of this current slot (increments each time this slot is used)
		uint32_t m_NextFree{ 0 };					// Points to next free slot index
		JobSpec m_JobSpec{ };						// The job spec for this run slot (used to queue jobs once dependencies are met)
	};

	struct JobHandle
	{
		uint32_t m_Index{ 0 };
		uint32_t m_Generation{ 0 };
	};

	using JobQueues = std::array<SimpleConcurrentQueue<uint32_t>, static_cast<size_t>(JobPriority::Count)>;
}