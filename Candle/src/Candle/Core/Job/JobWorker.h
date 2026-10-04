#pragma once

#include "JobTypes.h"

#include "Candle/Core/Threading/Thread.h"

namespace Candle {

	class JobSystem;

	// Align the JobWorker class to 64 bytes to avoid false sharing between threads and keep the cache lines clean for better performance
	class alignas(64) JobWorker
	{
	public:
		JobWorker(uint32_t index, JobSystem& jobSystem);
		~JobWorker() = default;

		// Delete copy constructor and assignment operator to prevent copying
		JobWorker(JobWorker&) = delete;
		JobWorker& operator=(JobWorker&) = delete;

		// Execute the job worker loop
		void Start();

		inline void RequestStop() { m_Thread.RequestStop(); }
		inline void Join() { m_Thread.Join(); }

		// Any thread. True only on a thread currently running a worker loop, of any JobSystem.
		static bool IsWorkerThread();

	private:
		void Execute(std::stop_token stoken);

	private:
		uint32_t m_Index{ 0 };
		JobSystem& m_JobSystem;
		Thread m_Thread;
	};

}