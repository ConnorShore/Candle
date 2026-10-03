#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Core/Job/JobSystem.h>

#include <atomic>
#include <thread>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Stress;

// The job system always runs on its own workers, so even the "unit" tests here are multi-threaded.
// They are tagged Unit because they are small and fast; the Stress ones hammer races on purpose.

namespace {

	// Owns a started JobSystem and stops it on scope exit, including when a CHECK throws.
	// Declare it *after* any state its jobs touch, so the workers are joined before that state dies.
	// Heap-allocated because the 4096-entry run-slot table is several hundred KB.
	class JobSystemFixture
	{
	public:
		JobSystemFixture()
		{
			// Every job logs on the Job channel; mute it so the tests measure scheduling, not std::format.
			Logger::SetChannelMask(static_cast<uint16_t>(0xFFFF & ~static_cast<uint16_t>(LogChannel::Job)));
			m_System = ScopedPtr<JobSystem>::Create();
			m_System->Start();
		}

		~JobSystemFixture()
		{
			m_System->Stop();
			Logger::SetChannelMask(0xFFFF);
		}

		JobSystemFixture(const JobSystemFixture&) = delete;
		JobSystemFixture& operator=(const JobSystemFixture&) = delete;

		JobSystem* operator->() const { return m_System.Get(); }

	private:
		ScopedPtr<JobSystem> m_System;
	};

	// Polls rather than calling WaitForJob, which has no timeout and so would hang the run on a lost job.
	// Yields rather than CDL_THREAD_PAUSE so the workers being waited on get the core.
	template<typename Pred>
	bool WaitUntil(Pred&& pred, uint32_t timeoutMs = 5000)
	{
		const Tick start = Platform::GetTick();
		const double budgetMs = timeoutMs * PerfScale();
		while (!pred())
		{
			if (Platform::ToMilliseconds(start, Platform::GetTick()) > budgetMs)
				return false;
			Platform::YieldCurrentThread();
		}
		return true;
	}

	// Entry completion is not slot release: FinishJob runs after the entry returns. Tests that
	// reason about a slot being recycled give the worker a moment to get through FinishJob.
	void LetFinishJobComplete() { Platform::SleepCurrentThread(20); }

	// m_FuncData points at one of these. Every job in a KickJobs batch receives the same pointer.
	struct Counter
	{
		std::atomic<uint32_t> Value{ 0 };
		uint32_t WorkMs = 0;

		uint32_t Get() const { return Value.load(std::memory_order_acquire); }
	};

	void CountEntry(uintptr_t data)
	{
		auto* counter = reinterpret_cast<Counter*>(data);
		if (counter->WorkMs > 0)
			Platform::SleepCurrentThread(counter->WorkMs);
		counter->Value.fetch_add(1, std::memory_order_acq_rel);
	}

	// Snapshots a watched counter when it starts, so a successor can prove its dependencies had finished.
	struct Observer
	{
		const Counter* Watched = nullptr;
		std::atomic<uint32_t> SeenAtStart{ 0 };
		std::atomic<uint32_t> Runs{ 0 };
	};

	void ObserveEntry(uintptr_t data)
	{
		auto* observer = reinterpret_cast<Observer*>(data);
		observer->SeenAtStart.store(observer->Watched->Get(), std::memory_order_relaxed);
		observer->Runs.fetch_add(1, std::memory_order_acq_rel);
	}

	// Stamps start/end ticks from a shared clock, so tests can assert happens-before across jobs.
	struct Stamped
	{
		std::atomic<uint32_t>* Clock = nullptr;
		uint32_t WorkMs = 0;
		std::atomic<uint32_t> Start{ 0 };
		std::atomic<uint32_t> End{ 0 };
		std::atomic<uint32_t> Runs{ 0 };
	};

	void StampEntry(uintptr_t data)
	{
		auto* job = reinterpret_cast<Stamped*>(data);
		job->Start.store(job->Clock->fetch_add(1), std::memory_order_relaxed);
		if (job->WorkMs > 0)
			Platform::SleepCurrentThread(job->WorkMs);
		job->End.store(job->Clock->fetch_add(1), std::memory_order_relaxed);
		job->Runs.fetch_add(1, std::memory_order_acq_rel);
	}

	template<typename T>
	JobSpec Spec(EntryFn fn, T* data, JobPriority priority = JobPriority::Normal)
	{
		return { .m_EntryFunc = fn, .m_FuncData = reinterpret_cast<uintptr_t>(data), .m_Priority = priority, .m_Name = "TestJob" };
	}

}

//////////////////////////////////////////////////////////////////////////
// Basic execution
//////////////////////////////////////////////////////////////////////////

CDL_TEST_CASE(JobSystem, StartAndStopWithNoWork, Unit)
{
	JobSystemFixture jobs;
}

CDL_TEST_CASE(JobSystem, SingleJobRunsOnce, Unit)
{
	Counter counter;
	JobSystemFixture jobs;

	jobs->KickJob(Spec(&CountEntry, &counter));

	CDL_CHECK_MSG(WaitUntil([&] { return counter.Get() >= 1; }), "job never ran");
	LetFinishJobComplete();
	CDL_CHECK_EQ(counter.Get(), 1u);
}

CDL_TEST_CASE(JobSystem, IndependentJobsEachRunOnce, Unit)
{
	constexpr uint32_t jobCount = 1000;
	std::vector<Counter> counters(jobCount);
	JobSystemFixture jobs;

	for (Counter& counter : counters)
		jobs->KickJob(Spec(&CountEntry, &counter));

	const bool allRan = WaitUntil([&] {
		return std::ranges::all_of(counters, [](const Counter& c) { return c.Get() >= 1; });
	});
	LetFinishJobComplete();

	uint32_t wrong = 0;
	for (const Counter& counter : counters)
		wrong += counter.Get() != 1 ? 1 : 0;

	CDL_CHECK_MSG(allRan, "at least one job never ran");
	CDL_CHECK_EQ(wrong, 0u);
}

CDL_TEST_CASE(JobSystem, EveryPriorityRuns, Unit)
{
	Counter low, normal, high;
	JobSystemFixture jobs;

	jobs->KickJob(Spec(&CountEntry, &low, JobPriority::Low));
	jobs->KickJob(Spec(&CountEntry, &normal, JobPriority::Normal));
	jobs->KickJob(Spec(&CountEntry, &high, JobPriority::High));

	CDL_CHECK(WaitUntil([&] { return low.Get() && normal.Get() && high.Get(); }));
}

//////////////////////////////////////////////////////////////////////////
// Batches
//////////////////////////////////////////////////////////////////////////

CDL_TEST_CASE(JobSystem, KickJobsRunsEntryExactlyNumJobsTimes, Unit)
{
	// Trivial jobs, so workers drain the queue while the batch is still being pushed.
	constexpr uint32_t batchSize = 64;
	Counter counter;
	JobSystemFixture jobs;

	jobs->KickJobs(batchSize, Spec(&CountEntry, &counter));

	const bool done = WaitUntil([&] { return counter.Get() >= batchSize; }, 2000);
	LetFinishJobComplete();
	CDL_CHECK_MSG(done, std::format("only {} of {} batch jobs ran", counter.Get(), batchSize));
	CDL_CHECK_EQ(counter.Get(), batchSize);
}

CDL_TEST_CASE(JobSystem, SuccessorWaitsForWholeBatch, Unit)
{
	constexpr uint32_t batchSize = 32;
	Counter batch{ .WorkMs = 2 };
	Observer successor{ .Watched = &batch };
	JobSystemFixture jobs;

	const JobHandle batchHandle = jobs->KickJobs(batchSize, Spec(&CountEntry, &batch));
	jobs->KickJob(Spec(&ObserveEntry, &successor), { batchHandle });

	CDL_CHECK_MSG(WaitUntil([&] { return successor.Runs.load() >= 1; }), "successor never ran");
	CDL_CHECK_EQ(successor.SeenAtStart.load(), batchSize);
}

//////////////////////////////////////////////////////////////////////////
// Dependencies
//////////////////////////////////////////////////////////////////////////

CDL_TEST_CASE(JobSystem, SuccessorRunsAfterDependency, Unit)
{
	std::atomic<uint32_t> clock{ 0 };
	Stamped first{ .Clock = &clock, .WorkMs = 20 };
	Stamped second{ .Clock = &clock };
	JobSystemFixture jobs;

	const JobHandle a = jobs->KickJob(Spec(&StampEntry, &first));
	jobs->KickJob(Spec(&StampEntry, &second), { a });

	CDL_CHECK_MSG(WaitUntil([&] { return second.Runs.load() >= 1; }), "successor never ran");
	CDL_EXPECT_LT(first.End.load(), second.Start.load());
}

CDL_TEST_CASE(JobSystem, SuccessorWaitsForEveryDependency, Unit)
{
	constexpr uint32_t depCount = 8;
	Counter deps{ .WorkMs = 5 };
	Observer successor{ .Watched = &deps };
	JobSystemFixture jobs;

	std::vector<JobHandle> handles;
	for (uint32_t i = 0; i < depCount; ++i)
		handles.push_back(jobs->KickJob(Spec(&CountEntry, &deps)));
	jobs->KickJob(Spec(&ObserveEntry, &successor), handles);

	CDL_CHECK_MSG(WaitUntil([&] { return successor.Runs.load() >= 1; }), "successor never ran");
	CDL_CHECK_EQ(successor.SeenAtStart.load(), depCount);
}

CDL_TEST_CASE(JobSystem, DiamondRunsInDependencyOrder, Unit)
{
	//     A
	//    / \
	//   B   C
	//    \ /
	//     D
	std::atomic<uint32_t> clock{ 0 };
	Stamped a{ .Clock = &clock, .WorkMs = 5 };
	Stamped b{ .Clock = &clock, .WorkMs = 5 };
	Stamped c{ .Clock = &clock, .WorkMs = 10 };
	Stamped d{ .Clock = &clock };
	JobSystemFixture jobs;

	const JobHandle ha = jobs->KickJob(Spec(&StampEntry, &a));
	const JobHandle hb = jobs->KickJob(Spec(&StampEntry, &b), { ha });
	const JobHandle hc = jobs->KickJob(Spec(&StampEntry, &c), { ha });
	jobs->KickJob(Spec(&StampEntry, &d), { hb, hc });

	CDL_CHECK_MSG(WaitUntil([&] { return d.Runs.load() >= 1; }), "D never ran");
	LetFinishJobComplete();

	CDL_EXPECT_LT(a.End.load(), b.Start.load());
	CDL_EXPECT_LT(a.End.load(), c.Start.load());
	CDL_EXPECT_LT(b.End.load(), d.Start.load());
	CDL_EXPECT_LT(c.End.load(), d.Start.load());
	for (const Stamped* job : { &a, &b, &c, &d })
		CDL_EXPECT_EQ(job->Runs.load(), 1u);
}

CDL_TEST_CASE(JobSystem, DependingOnFinishedJobStillRuns, Unit)
{
	// The common real-world case: the dependency finished before the successor was kicked.
	// Its handle is now stale, and a stale handle must mean "already complete", not "wait forever".
	Counter first;
	Counter second;
	JobSystemFixture jobs;

	const JobHandle a = jobs->KickJob(Spec(&CountEntry, &first));
	CDL_CHECK(WaitUntil([&] { return first.Get() >= 1; }));
	LetFinishJobComplete();

	jobs->KickJob(Spec(&CountEntry, &second), { a });
	CDL_CHECK_MSG(WaitUntil([&] { return second.Get() >= 1; }, 1000),
		"successor of an already-finished job never ran");
}

CDL_TEST_CASE(JobSystem, RecycledSlotGetsNewGeneration, Unit)
{
	Counter first;
	Counter second;
	JobSystemFixture jobs;

	const JobHandle a = jobs->KickJob(Spec(&CountEntry, &first));
	CDL_CHECK(WaitUntil([&] { return first.Get() >= 1; }));
	LetFinishJobComplete();

	const JobHandle b = jobs->KickJob(Spec(&CountEntry, &second));
	CDL_CHECK(WaitUntil([&] { return second.Get() >= 1; }));

	if (a.m_Index != b.m_Index)
		CDL_SKIP("free list did not hand back the same slot, so there is no reuse to check");
	CDL_CHECK_NE(a.m_Generation, b.m_Generation);
}

//////////////////////////////////////////////////////////////////////////
// Waiting and shutdown
//////////////////////////////////////////////////////////////////////////

CDL_TEST_CASE(JobSystem, WaitForJobBlocksUntilFinished, Unit)
{
	Counter counter{ .WorkMs = 50 };
	JobSystemFixture jobs;

	const JobHandle handle = jobs->KickJob(Spec(&CountEntry, &counter));
	jobs->WaitForJob(handle);

	CDL_CHECK_EQ(counter.Get(), 1u);
}

CDL_TEST_CASE(JobSystem, StopWithQueuedWorkReturns, Unit)
{
	// Stop drops queued work rather than draining it; this only checks that it does not hang.
	constexpr uint32_t jobCount = 2000;
	Counter counter{ .WorkMs = 1 };
	{
		JobSystemFixture jobs;
		for (uint32_t i = 0; i < jobCount; ++i)
			jobs->KickJob(Spec(&CountEntry, &counter));
	}
	CDL_NOTE(std::format("{} of {} jobs ran before Stop", counter.Get(), jobCount));
	CDL_EXPECT_LE(counter.Get(), jobCount);
}

//////////////////////////////////////////////////////////////////////////
// Stress
//////////////////////////////////////////////////////////////////////////

CDL_TEST_CASE(JobSystem, LongChainRunsInOrder, Stress)
{
	// Kicked as fast as possible, so each link's dependency is often finishing while the next is kicked.
	constexpr uint32_t chainLength = 1000;
	std::atomic<uint32_t> clock{ 0 };
	std::vector<Stamped> links(chainLength);
	for (Stamped& link : links)
		link.Clock = &clock;
	JobSystemFixture jobs;

	JobHandle previous = jobs->KickJob(Spec(&StampEntry, &links[0]));
	for (uint32_t i = 1; i < chainLength; ++i)
		previous = jobs->KickJob(Spec(&StampEntry, &links[i]), { previous });

	const bool done = WaitUntil([&] { return links.back().Runs.load() >= 1; });
	LetFinishJobComplete();

	uint32_t ran = 0, outOfOrder = 0, duplicated = 0;
	for (uint32_t i = 0; i < chainLength; ++i)
	{
		ran += links[i].Runs.load() >= 1 ? 1 : 0;
		duplicated += links[i].Runs.load() > 1 ? 1 : 0;
		if (i > 0 && links[i].Runs.load() && links[i - 1].End.load() > links[i].Start.load())
			++outOfOrder;
	}

	CDL_EXPECT_MSG(done, std::format("chain stalled: {} of {} links ran", ran, chainLength));
	CDL_EXPECT_EQ(outOfOrder, 0u);
	CDL_EXPECT_EQ(duplicated, 0u);
}

CDL_TEST_CASE(JobSystem, FanInRacesDependencyCompletion, Stress)
{
	// Trivial dependencies can finish between the successor's slot being claimed and its
	// dependency count being fully registered; the successor must still run once, after all of them.
	constexpr uint32_t rounds = 200;
	constexpr uint32_t depCount = 16;
	std::vector<Counter> deps(rounds);
	std::vector<Observer> successors(rounds);
	JobSystemFixture jobs;

	for (uint32_t round = 0; round < rounds; ++round)
	{
		successors[round].Watched = &deps[round];
		std::vector<JobHandle> handles;
		for (uint32_t i = 0; i < depCount; ++i)
			handles.push_back(jobs->KickJob(Spec(&CountEntry, &deps[round])));
		jobs->KickJob(Spec(&ObserveEntry, &successors[round]), handles);
	}

	const bool done = WaitUntil([&] {
		return std::ranges::all_of(successors, [](const Observer& o) { return o.Runs.load() >= 1; });
	});
	LetFinishJobComplete();

	uint32_t stalled = 0, early = 0, duplicated = 0;
	for (const Observer& successor : successors)
	{
		stalled += successor.Runs.load() == 0 ? 1 : 0;
		duplicated += successor.Runs.load() > 1 ? 1 : 0;
		early += successor.Runs.load() && successor.SeenAtStart.load() != depCount ? 1 : 0;
	}

	CDL_EXPECT_MSG(done, std::format("{} of {} successors never ran", stalled, rounds));
	CDL_EXPECT_EQ(early, 0u);
	CDL_EXPECT_EQ(duplicated, 0u);
}

CDL_TEST_CASE(JobSystem, SlotRecyclingKeepsDependencies, Stress)
{
	// 6000 slots through a 4096-slot table, so slots that previously had successors get reused.
	// Each wave is drained before the next, so the table never actually fills.
	constexpr uint32_t waves = 30;
	constexpr uint32_t pairsPerWave = 100;
	std::atomic<uint32_t> clock{ 0 };
	std::vector<Stamped> firsts(waves * pairsPerWave), seconds(waves * pairsPerWave);
	for (uint32_t i = 0; i < firsts.size(); ++i)
	{
		firsts[i].Clock = &clock;
		seconds[i].Clock = &clock;
	}
	JobSystemFixture jobs;

	uint32_t stalledWave = waves;
	for (uint32_t wave = 0; wave < waves && stalledWave == waves; ++wave)
	{
		const uint32_t begin = wave * pairsPerWave, end = begin + pairsPerWave;
		for (uint32_t i = begin; i < end; ++i)
		{
			const JobHandle a = jobs->KickJob(Spec(&StampEntry, &firsts[i]));
			jobs->KickJob(Spec(&StampEntry, &seconds[i]), { a });
		}

		if (!WaitUntil([&] { for (uint32_t i = begin; i < end; ++i) if (!seconds[i].Runs.load()) return false; return true; }, 2000))
			stalledWave = wave;
		LetFinishJobComplete();
	}

	uint32_t outOfOrder = 0, duplicated = 0;
	for (uint32_t i = 0; i < firsts.size(); ++i)
	{
		duplicated += (firsts[i].Runs.load() > 1 || seconds[i].Runs.load() > 1) ? 1 : 0;
		if (seconds[i].Runs.load() && (!firsts[i].Runs.load() || firsts[i].End.load() > seconds[i].Start.load()))
			++outOfOrder;
	}

	CDL_EXPECT_MSG(stalledWave == waves, std::format("wave {} of {} stalled", stalledWave, waves));
	CDL_EXPECT_EQ(outOfOrder, 0u);
	CDL_EXPECT_EQ(duplicated, 0u);
}

CDL_TEST_CASE(JobSystem, ConcurrentKickersLoseNothing, Stress)
{
	// Several threads kick at once, contending on the free-slot lock and the queues.
	constexpr uint32_t perThread = 200;
	const int threadCount = StressThreadCount();
	std::vector<Counter> counters(threadCount);
	JobSystemFixture jobs;

	RunConcurrently(threadCount, [&](int thread) {
		for (uint32_t i = 0; i < perThread; ++i)
			jobs->KickJob(Spec(&CountEntry, &counters[thread]));
	});

	const bool done = WaitUntil([&] {
		return std::ranges::all_of(counters, [](const Counter& c) { return c.Get() >= perThread; });
	});
	LetFinishJobComplete();

	CDL_EXPECT(done);
	for (int thread = 0; thread < threadCount; ++thread)
		CDL_EXPECT_EQ(counters[thread].Get(), perThread);
}
