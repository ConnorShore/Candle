#include "cdlpch.h"
#include "JobWorker.h"
#include "JobSystem.h"

namespace Candle {

	// Set for the lifetime of a worker loop so blocking calls can refuse to run on a worker.
	static thread_local bool t_IsWorkerThread = false;

	bool JobWorker::IsWorkerThread()
	{
		return t_IsWorkerThread;
	}

	// For  now all threads will be created with the same priority and no affinity mask (allowing the OS to schedule them on any available core)
	// In future may have "high priority" threads that are given higher priority and/or pinned to specific cores
	// Will determine best approach once the job system is working and we can profile it to see if there are any bottlenecks or issues with thread scheduling
	JobWorker::JobWorker(uint32_t index, JobSystem& jobSystem)
		: m_Index(index), m_JobSystem(jobSystem), m_Thread("JobWorker_" + std::to_string(index))
	{
	}

	void JobWorker::Start()
	{
		m_Thread.Start([this](std::stop_token stoken) { Execute(stoken); });
	}

	void JobWorker::Execute(std::stop_token stoken)
	{
		t_IsWorkerThread = true;

		while (!stoken.stop_requested())
		{
			bool jobFound = false;
			// Check for jobs in the queues based on priority
			for (JobPriority priority : kPriorityOrder)
			{
				std::optional<JobRunDecl> jobDeclOp = m_JobSystem.TryPopJob(priority);
				if (jobDeclOp.has_value())
				{
					jobFound = true;

					const JobRunDecl decl = jobDeclOp.value();
					const JobRunSlot& slot = m_JobSystem.m_JobRunSlots[decl.m_RunSlotIndex];

					// Safe to read without the lock: the slot can't be recycled until this chunk calls FinishJob.
					const JobSpec jobSpec = slot.m_JobSpec;
					const uint32_t end = std::min(decl.m_FirstIndex + slot.m_ChunkSize, slot.m_BatchSize);

					// Execute every index in this chunk; one zone per chunk, not per index, keeps the trace readable.
					{
						CDL_PROFILE_SCOPE("Job");
						CDL_PROFILE_SCOPE_NAME(jobSpec.m_Name);
						CDL_PROFILE_SCOPE_METADATA(end - decl.m_FirstIndex);	// Indices in this chunk, to spot imbalance

						for (uint32_t index = decl.m_FirstIndex; index < end; ++index)
							jobSpec.m_EntryFunc(jobSpec.m_FuncData, index);
					}

					// Once the whole chunk is done, count it towards finishing the slot
					m_JobSystem.FinishJob(decl.m_RunSlotIndex);

					break; // Exit the priority loop to check for jobs again from the highest priority
				}
			}

			if (!jobFound)
				CDL_THREAD_PAUSE(); // Pause the thread to avoid busy waiting
		}
	}

}