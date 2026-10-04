#pragma once

#include "JobTypes.h"
#include "JobWorker.h"

#include "Candle/Core/Memory/ScopedPtr.h"
#include "Candle/Core/Threading/SpinLock.h"

#include <initializer_list>
#include <optional>
#include <span>

namespace Candle {

	class JobSystem
	{
	public:
		JobSystem();
		~JobSystem() = default;

		void Start();
		void Stop();

		JobHandle KickJob(JobSpec spec, std::span<const JobHandle> deps = {});
		JobHandle KickJob(JobSpec spec, std::initializer_list<JobHandle> deps);
		JobHandle KickJobs(uint32_t numJobs, JobSpec spec, std::initializer_list<JobHandle> deps);
		JobHandle KickJobs(uint32_t numJobs, JobSpec spec, std::span<const JobHandle> deps = {});

		// Blocks the calling thread until the job's slot is recycled; returns at once for an invalid handle.
		// Any non-worker thread. Never call from inside a job: if every worker blocks on queued work, nothing can run it (asserted).
		void WaitForJob(JobHandle job);

	private:
		void QueueJobSlot(uint32_t slotIndex);
		void FinishJob(uint32_t jobRunSlotIndex);	// One chunk done; the last one calls FinishSlot
		void FinishSlot(uint32_t jobRunSlotIndex);	// Frees the slot and releases its successors

		std::optional<JobRunDecl> TryPopJob(JobPriority priority);

	private:
		static constexpr size_t k_MaxJobRunSlots = 4096;

		friend class JobWorker;

	private:
		std::array<JobRunSlot, k_MaxJobRunSlots> m_JobRunSlots;
		uint32_t m_FreeSlot = 0;
		SpinLock m_SlotLock;

		JobQueues m_JobQueues;

		std::vector<ScopedPtr<JobWorker>> m_Workers;	// Will reserve and populate based on NumProcessors - 2 (exclude main + render threads)
	};

}
