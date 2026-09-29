#include "TestFramework.h"
#include "TestHelpers.h"

#include "Candle/Core/Threading/Thread.h"

#include <atomic>
#include <deque>
#include <memory>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Stress;

CDL_TEST_CASE(Thread, StartAndJoin, Unit)
{
	Thread thread("TestThread");
	bool executed = false;
	thread.Start([&executed]() {
		executed = true;
		});
	thread.Join();
	CDL_CHECK(executed);
}

CDL_TEST_CASE(Thread, StartAndJoinWithStopToken, Unit)
{
	Thread thread("TestThread");
	bool executed = false;
	thread.Start([&executed](std::stop_token) {
		executed = true;
		});
	thread.Join();
	CDL_CHECK(executed);
}

CDL_TEST_CASE(Thread, StopRequest, Unit)
{
	Thread thread("TestThread");
	std::atomic<bool> stopRequested = false;
	thread.Start([&stopRequested](std::stop_token stoken) {
		while (!stoken.stop_requested())
		{
			// Simulate work
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		stopRequested = true;
		});
	thread.StopRequest();
	thread.Join();
	CDL_CHECK(stopRequested);
}

CDL_TEST_CASE(Thread, LValueFunctionParams, Unit)
{
	Thread thread("TestThread");
	int value = 0;
	thread.Start([&value](std::stop_token, int& val) {
		val = 42;
		}, std::ref(value));
	thread.Join();
	CDL_CHECK_EQ(value, 42);
}

CDL_TEST_CASE(Thread, RValueFunctionParams, Unit)
{
	Thread thread("TestThread");
	int value = 0;
	thread.Start([&value](std::stop_token, int val) {
		value = val;
		}, 42);
	thread.Join();
	CDL_CHECK_EQ(value, 42);
}

CDL_TEST_CASE(Thread, NamedArgumentIsCopied, Unit)
{
	Thread thread("TestThread");
	int input = 42;
	int received = 0;
	thread.Start([&received](int val) { received = val; }, input);
	thread.Join();
	CDL_CHECK_EQ(received, 42);
}

CDL_TEST_CASE(Thread, EnsureCopyFails, Unit)
{
	CDL_CHECK_FALSE(std::is_copy_constructible_v<Thread>);
	CDL_CHECK_FALSE(std::is_copy_assignable_v<Thread>);
}

// Compile-time: the concept must agree with what Start can actually call, or it lets through calls that fail inside <thread>.
CDL_STATIC_ASSERT(ThreadFunc<void(*)(int), int&>);                                  // a named variable is copied in
CDL_STATIC_ASSERT(ThreadFunc<void(*)(int&&), int&>);                                // ...and that copy is moved into the call
CDL_STATIC_ASSERT(ThreadFunc<void(*)(std::stop_token, int), int&>);
CDL_STATIC_ASSERT(ThreadFunc<void(*)(int&), std::reference_wrapper<int>>);          // std::ref is the only way to bind a reference
CDL_STATIC_ASSERT(!ThreadFunc<void(*)(int&), int&>);
CDL_STATIC_ASSERT(!ThreadFunc<void(*)(int)>);                                       // missing argument
CDL_STATIC_ASSERT(!ThreadFunc<void(*)(), int>);                                     // extra argument

CDL_TEST_CASE(Thread, MoveOnlyArgument, Unit)
{
	Thread thread("TestThread");
	auto input = std::make_unique<int>(42);
	int received = 0;
	thread.Start([&received](std::unique_ptr<int> ptr) { received = *ptr; }, std::move(input));
	thread.Join();
	CDL_CHECK(input == nullptr);
	CDL_CHECK_EQ(received, 42);
}

CDL_TEST_CASE(Thread, MutableCallable, Unit)
{
	Thread thread("TestThread");
	int result = 0;
	thread.Start([&result, calls = 0]() mutable { result = ++calls; });
	thread.Join();
	CDL_CHECK_EQ(result, 1);
}

CDL_TEST_CASE(Thread, GetIdMatchesIdSeenInsideThread, Unit)
{
	Thread thread("TestThread");
	std::atomic<uint32_t> insideId = 0;
	thread.Start([&insideId] { insideId.store(Platform::GetCurrentThreadId(), std::memory_order_release); });

	// The handle outlives the body; only Join() closes it, so GetId is valid until then.
	while (insideId.load(std::memory_order_acquire) == 0)
		std::this_thread::yield();

	CDL_CHECK_EQ(thread.GetId(), insideId.load());
	CDL_CHECK_NE(thread.GetId(), Platform::GetCurrentThreadId());
	thread.Join();
}

CDL_TEST_CASE(Thread, GetIdIsZeroWhenNotRunning, Unit)
{
	Thread thread("TestThread");
	CDL_CHECK_EQ(thread.GetId(), 0u);
	thread.Start([] {});
	thread.Join();
	CDL_CHECK_EQ(thread.GetId(), 0u);
}

CDL_TEST_CASE(Thread, JoinAndStopBeforeStartAreNoOps, Unit)
{
	Thread thread("TestThread");
	thread.StopRequest();
	thread.Join();
	thread.Join();
	CDL_CHECK_EQ(thread.GetId(), 0u);
}

CDL_TEST_CASE(Thread, RestartAfterJoin, Unit)
{
	Thread thread("TestThread");
	int runs = 0;
	for (int i = 0; i < 3; ++i)
	{
		thread.Start([&runs] { ++runs; });
		thread.Join();
	}
	CDL_CHECK_EQ(runs, 3);
}

// Fails by hanging rather than by assertion: if ~Thread joins without requesting stop, the body never exits.
CDL_TEST_CASE(Thread, DestructorRequestsStopAndJoins, Unit)
{
	std::atomic<bool> exited = false;
	{
		Thread thread("TestThread");
		thread.Start([&exited](std::stop_token stoken) {
			while (!stoken.stop_requested())
				std::this_thread::yield();
			exited = true;
		});
	}
	CDL_CHECK(exited);
}

CDL_TEST_CASE(Thread, AffinityMaskPinsToCore, Unit)
{
	Thread thread(ThreadDescriptor{ .Name = "PinnedThread", .AffinityMask = 1 });
	bool alwaysOnCoreZero = true;
	thread.Start([&alwaysOnCoreZero] {
		// Yielding invites the scheduler to migrate the thread, which the mask must prevent.
		for (int i = 0; i < 1'000; ++i)
		{
			if (Platform::GetCurrentThreadProcessor() != 0)
				alwaysOnCoreZero = false;
			std::this_thread::yield();
		}
	});
	thread.Join();
	CDL_CHECK(alwaysOnCoreZero);
}

CDL_TEST_CASE(Thread, ManyThreadsStopOnDestruction, Stress)
{
	constexpr int kThreads = 64;
	constexpr int kRounds = 50;
	std::atomic<int> started = 0;
	std::atomic<int> stopped = 0;

	for (int round = 0; round < kRounds; ++round)
	{
		// deque, because Thread is immovable and vector growth would need to move it.
		std::deque<Thread> threads;
		for (int i = 0; i < kThreads; ++i)
		{
			threads.emplace_back("StressThread");
			threads.back().Start([&started, &stopped](std::stop_token stoken) {
				started.fetch_add(1, std::memory_order_relaxed);
				while (!stoken.stop_requested())
					std::this_thread::yield();
				stopped.fetch_add(1, std::memory_order_relaxed);
			});
		}
	}

	CDL_CHECK_EQ(started.load(), kThreads * kRounds);
	CDL_CHECK_EQ(stopped.load(), kThreads * kRounds);
}
