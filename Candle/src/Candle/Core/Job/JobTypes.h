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
		EntryFn EntryFunc{ nullptr };
		uintptr_t FuncData{ 0 };
		JobPriority Priority{ JobPriority::Normal };
		const char* Name{ "Job" };
	};

	// TODO: Once working, see if we can simplify or trim this down to a smaller size
	struct JobRunSlot
	{
		std::atomic<uint32_t> JobCount{ 0 };		// Chunks still to finish; the chunk that takes this to 0 finishes the slot
		uint32_t BatchSize{ 0 };					// Indices in the batch; written before the slot is queued, read-only after
		// TODO: Find a avoid a vector here due to heap allocs in lock; we could use a fixed-size array of 4 or 8, and then allocate a vector only if we exceed that
		std::vector<uint32_t> Successors{ };		// Job Slot indicies depending on this run slot finishing (should be notified once this is done)
		std::atomic<uint32_t> Remaining{ 0 };		// Unfinished dependencies; the slot is queued when this reaches 0
		std::atomic<uint32_t> Generation{ 0 };		// Generation number of this current slot (increments each time this slot is used)
		uint32_t NextFree{ 0 };						// Points to next free slot index
		uint32_t ChunkSize{ 0 };					// Indices per chunk (the last may be short); written before queueing, read-only after
		JobSpec Spec{ };							// The job spec for this run slot (used to queue jobs once dependencies are met)
	};

	// One queue entry: a contiguous range of a slot's batch, [FirstIndex, FirstIndex + ChunkSize).
	struct JobRunDecl
	{
		uint32_t FirstIndex{ 0 };		// First batch index in this chunk
		uint32_t RunSlotIndex{ 0 };		// Index of the run slot this chunk belongs to
	};

	constexpr uint32_t kInvalidJobRunSlotIndex = std::numeric_limits<uint32_t>::max();

	// Default-constructed handles are invalid, so "no job" can't alias slot 0. As a dependency an
	// invalid handle is ignored, and WaitForJob on one returns immediately.
	struct JobHandle
	{
		uint32_t Index{ kInvalidJobRunSlotIndex };
		uint32_t Generation{ 0 };

		constexpr bool IsValid() const { return Index != kInvalidJobRunSlotIndex; }
	};

	using JobQueues = std::array<SimpleConcurrentQueue<JobRunDecl>, static_cast<size_t>(JobPriority::Count)>;
}