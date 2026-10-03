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
				std::optional<uint32_t> slotIndex = m_JobSystem.TryPopJob(priority);
				if (slotIndex.has_value())
				{
					jobFound = true;

					JobSpec jobSpec = m_JobSystem.GetJobSpec(slotIndex.value());

					CDL_CORE_INFO(LogChannel::Job, "JobWorker {} executing job: {} with priority: {}", m_Index, jobSpec.m_Name, static_cast<int>(jobSpec.m_Priority));

					// Execute the job
					jobSpec.m_EntryFunc(jobSpec.m_FuncData);

					// Once finished, finish the job
					m_JobSystem.FinishJob(slotIndex.value());

					break; // Exit the priority loop to check for jobs again from the highest priority
				}
			}

			if (!jobFound)
				CDL_THREAD_PAUSE(); // Pause the thread to avoid busy waiting
		}
	}

}