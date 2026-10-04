#pragma once

#include "Candle/Core/Memory/Collections/SimpleConcurrentQueue.h"

#include <atomic>
#include <array>
#include <functional>
#include <limits>
#include <string>

namespace Candle {

	// Every job takes its user data and its index within the batch; a single KickJob runs index 0.
	using EntryFn = void(*)(uintptr_t data, uint32_t index);

	enum class JobPriority : uint8_t
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
		std::atomic<uint32_t> m_JobCount{ 0 };		// Chunks still to finish; the chunk that takes this to 0 finishes the slot
		uint32_t m_BatchSize{ 0 };					// Indices in the batch; written before the slot is queued, read-only after
		std::vector<uint32_t> m_Successors{ };		// Job Slot indicies depending on this run slot finishing (should be notified once this is done)
		std::atomic<uint32_t> m_Remaining{ 0 };		// Unfinished dependencies; the slot is queued when this reaches 0
		std::atomic<uint32_t> m_Generation{ 0 };	// Generation number of this current slot (increments each time this slot is used)
		uint32_t m_NextFree{ 0 };					// Points to next free slot index
		uint32_t m_ChunkSize{ 0 };					// Indices per chunk (the last may be short); written before queueing, read-only after
		JobSpec m_JobSpec{ };						// The job spec for this run slot (used to queue jobs once dependencies are met)
	};

	// One queue entry: a contiguous range of a slot's batch, [m_FirstIndex, m_FirstIndex + m_ChunkSize).
	struct JobRunDecl
	{
		uint32_t m_FirstIndex{ 0 };		// First batch index in this chunk
		uint32_t m_RunSlotIndex{ 0 };	// Index of the run slot this chunk belongs to
	};

	constexpr uint32_t k_InvalidJobRunSlotIndex = std::numeric_limits<uint32_t>::max();

	// Default-constructed handles are invalid, so "no job" can't alias slot 0. As a dependency an
	// invalid handle is ignored, and WaitForJob on one returns immediately.
	struct JobHandle
	{
		uint32_t m_Index{ k_InvalidJobRunSlotIndex };
		uint32_t m_Generation{ 0 };

		constexpr bool IsValid() const { return m_Index != k_InvalidJobRunSlotIndex; }
	};

	using JobQueues = std::array<SimpleConcurrentQueue<JobRunDecl>, static_cast<size_t>(JobPriority::Count)>;
}