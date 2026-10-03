#include "cdlpch.h"
#include "JobSystem.h"

#include "Candle/Platform/Platform.h"

namespace Candle {

	constexpr uint32_t k_InvalidJobRunSlotIndex = std::numeric_limits<uint32_t>::max();

	JobSystem::JobSystem()
	{
		// Populate run slots
		for (uint32_t i = 0; i < k_MaxJobRunSlots - 1; ++i)
			m_JobRunSlots[i].m_NextFree = i + 1;

		// Set last slot's next free to invalid
		m_JobRunSlots[k_MaxJobRunSlots - 1].m_NextFree = k_InvalidJobRunSlotIndex;

		// Create workers based on number of processors - 2 (main + render threads)
		auto cpuTypology = Platform::QueryCPUTopology();
		auto numWorkers = std::max(1u, cpuTypology.NumLogicalCores - 2);

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
		// Acquire the free slot lock to safely access the free slot index
		m_SlotLock.Acquire();

		uint32_t freeSlotIndex = m_FreeSlot;
		if (freeSlotIndex == k_InvalidJobRunSlotIndex)
		{
			m_SlotLock.Release();
			CDL_CORE_ASSERT(false, "No free job run slots available!");
			return { k_InvalidJobRunSlotIndex, 0 };
		}
		JobRunSlot& slot = m_JobRunSlots[freeSlotIndex];
		m_FreeSlot = slot.m_NextFree;

		uint32_t generation = slot.m_Generation;

		slot.m_JobCount.store(numJobs);	// Run 1 job for this slot
		slot.m_JobSpec = spec;
		int numDeps = 0;
		for (const auto& dep : deps)
		{
			// If dependency is finished, skip it
			if (dep.m_Generation < m_JobRunSlots[dep.m_Index].m_Generation.load())
				continue;

			m_JobRunSlots[dep.m_Index].m_Successors.push_back(freeSlotIndex);
			numDeps++;
		}
		slot.m_Remaining.store(numDeps, std::memory_order_release);	// Set the number of remaining dependencies for this slot

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

		JobRunSlot& slot = m_JobRunSlots[job.m_Index];
		uint32_t gen;
		while ((gen = slot.m_Generation.load(std::memory_order_acquire)) <= job.m_Generation)
			slot.m_Generation.wait(gen, std::memory_order_acquire);
	}

	void JobSystem::FinishJob(uint32_t jobRunSlotIndex)
	{
		JobRunSlot& slot = m_JobRunSlots[jobRunSlotIndex];

		// Decrement the job count for this slot, if more jobs remain, we aren't fully done yet
		if (slot.m_JobCount.fetch_sub(1) > 1)
			return;

		CDL_CORE_WARN(LogChannel::Job, "JobRunSlot {} finished all jobs. Notifying successors.", jobRunSlotIndex);

		// Aquire lock and free the slot for reuse
		m_SlotLock.Acquire();
		auto successors = std::move(slot.m_Successors);	// Move successors out of the slot to avoid holding the lock while notifying them
		slot.m_Remaining.store(0);	// Reset remaining dependencies for this slot
		slot.m_Generation.fetch_add(1);	// Increment generation for this slot
		slot.m_NextFree = m_FreeSlot;
		m_FreeSlot = jobRunSlotIndex;
		m_SlotLock.Release();

		// Notify any threads waiting on this slot's generation that it has changed
		slot.m_Generation.notify_all();

		// Notify successors that this job run slot is fully finished
		for (uint32_t successorIndex : successors)
		{
			JobRunSlot& successorSlot = m_JobRunSlots[successorIndex];
			if (successorSlot.m_Remaining.fetch_sub(1) == 1)
			{
				// All dependencies for the successor slot are completed, queue its jobs
				QueueJobSlot(successorIndex);
			}
		}

		CDL_CORE_INFO(LogChannel::Job, "JobRunSlot {} completed. Generation: {}", jobRunSlotIndex, slot.m_Generation.load());
	}

	std::optional<uint32_t> JobSystem::TryPopJob(JobPriority priority)
	{
		return m_JobQueues[static_cast<size_t>(priority)].TryPop();
	}

	void JobSystem::QueueJobSlot(uint32_t slotIndex)
	{
		JobPriority priority = m_JobRunSlots[slotIndex].m_JobSpec.m_Priority;

		CDL_CORE_INFO(LogChannel::Job, "Queueing Job {} and job count: {}", m_JobRunSlots[slotIndex].m_JobSpec.m_Name, m_JobRunSlots[slotIndex].m_JobCount.load());

		// Push the slot index into the job queue for the specified priority, once for each job in the slot
		m_SlotLock.Acquire();
		uint32_t jobCt = m_JobRunSlots[slotIndex].m_JobCount.load();
		m_SlotLock.Release();

		for (uint32_t i = 0; i < jobCt; ++i)
			m_JobQueues[static_cast<size_t>(priority)].Push(slotIndex);
	}

}