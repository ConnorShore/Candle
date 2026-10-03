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

		// Blocks the calling thread until the job's slot is recycled. Any non-worker thread.
		// Never call from inside a job: if every worker blocks on queued work, nothing can run it (asserted).
		void WaitForJob(JobHandle job);
		void FinishJob(uint32_t jobRunSlotIndex);

		std::optional<uint32_t> TryPopJob(JobPriority priority);

		inline JobSpec GetJobSpec(uint32_t jobRunSlotIndex) const { return m_JobRunSlots[jobRunSlotIndex].m_JobSpec; }

	private:
		void QueueJobSlot(uint32_t slotIndex);

	private:
		static constexpr size_t k_MaxJobRunSlots = 4096;

	private:
		std::array<JobRunSlot, k_MaxJobRunSlots> m_JobRunSlots;
		uint32_t m_FreeSlot = 0;
		SpinLock m_SlotLock;

		JobQueues m_JobQueues;

		std::vector<ScopedPtr<JobWorker>> m_Workers;	// Will reserve and populate based on NumProcessors - 2 (exclude main + render threads)
	};

}
