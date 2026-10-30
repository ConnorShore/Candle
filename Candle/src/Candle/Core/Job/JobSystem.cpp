#include "cdlpch.h"
#include "JobSystem.h"

#include "Candle/Platform/Platform.h"

namespace Candle {

	JobSystem::JobSystem()
	{
		// Populate run slots
		for (uint32_t i = 0; i < kMaxJobRunSlots - 1; ++i)
			m_JobRunSlots[i].NextFree = i + 1;

		// Set last slot's next free to invalid
		m_JobRunSlots[kMaxJobRunSlots - 1].NextFree = kInvalidJobRunSlotIndex;

		// Create workers based on number of processors - 2 (main + render threads)
		auto cpuTopology = Platform::QueryCPUTopology();
		const uint32_t numWorkers = cpuTopology.NumLogicalCores > 2 ? cpuTopology.NumLogicalCores - 2 : 1;

		m_Workers.reserve(numWorkers);
		for (uint32_t i = 0; i < numWorkers; ++i)
			m_Workers.emplace_back(ScopedPtr<JobWorker>::Create(i, *this));
	}

	void JobSystem::Start()
	{
		for (auto& worker : m_Workers)
			worker->Start();
	}

	void JobSystem::Stop()
	{
		for (auto& worker : m_Workers)
			worker->RequestStop();

		for (auto& worker : m_Workers)
			worker->Join();
	}

	JobHandle JobSystem::KickJob(JobSpec spec, std::span<const JobHandle> deps /* ={} */)
	{
		return KickJobs(1, spec, deps);
	}

	JobHandle JobSystem::KickJob(JobSpec spec, std::initializer_list<JobHandle> deps)
	{
		return KickJob(spec, std::span<const JobHandle>(deps.begin(), deps.size()));
	}

	JobHandle JobSystem::KickJobs(uint32_t numJobs, JobSpec spec, std::initializer_list<JobHandle> deps)
	{
		return KickJobs(numJobs, spec, std::span<const JobHandle>(deps.begin(), deps.size()));
	}

	JobHandle JobSystem::KickJobs(uint32_t numJobs, JobSpec spec, std::span<const JobHandle> deps /* ={} */)
	{
		// An empty batch runs nothing, so it may omit the entry function and act as a pure join node.
		CDL_CORE_ASSERT(numJobs == 0 || spec.EntryFunc, "JobSpec must have an entry function");

		CDL_PROFILE_SCOPE("KickJobs");
		CDL_PROFILE_SCOPE_TEXT(spec.Name);
		CDL_PROFILE_SCOPE_METADATA(numJobs);

		// About four chunks per worker: few queue operations, but enough slack that one slow chunk doesn't stall the batch.
		// The max(1u, ...) keeps numJobs == 0 from dividing by zero; it yields chunkCount == 0.
		const uint32_t maxChunks = static_cast<uint32_t>(m_Workers.size()) * 4;
		const uint32_t chunkSize = std::max(1u, (numJobs + maxChunks - 1) / maxChunks);
		const uint32_t chunkCount = (numJobs + chunkSize - 1) / chunkSize;

		// Acquire the free slot lock to safely access the free slot index
		m_SlotLock.Acquire();

		uint32_t freeSlotIndex = m_FreeSlot;
		if (freeSlotIndex == kInvalidJobRunSlotIndex)
		{
			m_SlotLock.Release();
			CDL_CORE_ASSERT(false, "No free job run slots available!");
			return { kInvalidJobRunSlotIndex, 0 };
		}
		JobRunSlot& slot = m_JobRunSlots[freeSlotIndex];
		m_FreeSlot = slot.NextFree;

		uint32_t generation = slot.Generation;

		slot.JobCount.store(chunkCount);
		slot.BatchSize = numJobs;
		slot.ChunkSize = chunkSize;
		slot.Spec = spec;
		int numDeps = 0;
		for (const auto& dep : deps)
		{
			// An invalid handle is "no job", e.g. a failed kick or a default-constructed member
			if (!dep.IsValid())
				continue;
			CDL_CORE_ASSERT(dep.Index < kMaxJobRunSlots, "JobHandle index out of range");

			// If dependency is finished, skip it
			if (dep.Generation < m_JobRunSlots[dep.Index].Generation.load())
				continue;

			m_JobRunSlots[dep.Index].Successors.push_back(freeSlotIndex);
			numDeps++;
		}
		slot.Remaining.store(numDeps, std::memory_order_release);	// Set the number of remaining dependencies for this slot

		// Release the lock after updating the free slot index
		m_SlotLock.Release();

		// If deps are empty, queue job, otherwise it will be queued when deps are completed
		if (numDeps == 0)
			QueueJobSlot(freeSlotIndex);

		return { freeSlotIndex, generation };
	}

	void JobSystem::WaitForJob(JobHandle job)
	{
		// TODO: Can probably optimize this to have thread run jobs or something while waiting
		// Need to look into solutions in future if this becomes a bottleneck
		CDL_CORE_ASSERT(!JobWorker::IsWorkerThread(), "WaitForJob called from a job worker; blocking a worker can deadlock the pool");

		if (!job.IsValid())
			return;
		CDL_CORE_ASSERT(job.Index < kMaxJobRunSlots, "JobHandle index out of range");

		// The time a non-worker thread spends blocked on the pool: on the game thread, this is the frame's critical path.
		CDL_PROFILE_SCOPE("WaitForJob");

		JobRunSlot& slot = m_JobRunSlots[job.Index];
		uint32_t gen;
		while ((gen = slot.Generation.load(std::memory_order_acquire)) <= job.Generation)
			slot.Generation.wait(gen, std::memory_order_acquire);
	}

	void JobSystem::FinishJob(uint32_t jobRunSlotIndex)
	{
		JobRunSlot& slot = m_JobRunSlots[jobRunSlotIndex];

		// Decrement the job count for this slot, if more jobs remain, we aren't fully done yet
		if (slot.JobCount.fetch_sub(1) > 1)
			return;

		FinishSlot(jobRunSlotIndex);
	}

	void JobSystem::FinishSlot(uint32_t jobRunSlotIndex)
	{
		JobRunSlot& slot = m_JobRunSlots[jobRunSlotIndex];

		// The gap between a batch's last chunk and its successors starting; includes the slot lock and requeueing.
		CDL_PROFILE_SCOPE("FinishSlot");
		CDL_PROFILE_SCOPE_TEXT(slot.Spec.Name);

		// Aquire lock and free the slot for reuse
		m_SlotLock.Acquire();
		auto successors = std::move(slot.Successors);	// Move successors out of the slot to avoid holding the lock while notifying them
		slot.Remaining.store(0);						// Reset remaining dependencies for this slot
		slot.Generation.fetch_add(1);					// Increment generation for this slot
		slot.NextFree = m_FreeSlot;
		m_FreeSlot = jobRunSlotIndex;
		m_SlotLock.Release();

		// Notify any threads waiting on this slot's generation that it has changed
		slot.Generation.notify_all();

		// Notify successors that this job run slot is fully finished
		for (uint32_t successorIndex : successors)
		{
			JobRunSlot& successorSlot = m_JobRunSlots[successorIndex];
			if (successorSlot.Remaining.fetch_sub(1) == 1)
			{
				// All dependencies for the successor slot are completed, queue its jobs
				QueueJobSlot(successorIndex);
			}
		}
	}

	std::optional<JobRunDecl> JobSystem::TryPopJob(JobPriority priority)
	{
		return m_JobQueues[static_cast<size_t>(priority)].TryPop();
	}

	void JobSystem::QueueJobSlot(uint32_t slotIndex)
	{
		const JobRunSlot& slot = m_JobRunSlots[slotIndex];
		const uint32_t chunkCount = slot.JobCount.load();
		const uint32_t chunkSize = slot.ChunkSize;

		// An empty batch has no chunk to finish it, so it completes as soon as its dependencies have.
		if (chunkCount == 0)
		{
			FinishSlot(slotIndex);
			return;
		}

		m_JobQueues[static_cast<size_t>(slot.Spec.Priority)].PushGenerated(chunkCount, [=](uint32_t chunk) {
			return JobRunDecl{ .FirstIndex = chunk * chunkSize, .RunSlotIndex = slotIndex };
		});
	}

}