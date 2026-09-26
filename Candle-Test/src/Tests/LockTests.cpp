#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Core/Threading/AdaptiveSpinLock.h>
#include <Candle/Core/Threading/ReentrantLock.h>
#include <Candle/Core/Threading/ScopedLock.h>
#include <Candle/Core/Threading/SpinLock.h>

#include <mutex>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Stress;

namespace {

	// A thread that is not the holder, which is the only way to observe exclusion for a reentrant lock.
	template<typename Lock>
	bool TryAcquireFromOtherThread(Lock& lock)
	{
		bool acquired = false;
		std::jthread([&] {
			acquired = lock.TryAcquire();
			if (acquired)
				lock.Release();
		}).join();
		return acquired;
	}

	template<typename Lock>
	void CheckTryAcquireExcludesOtherThreads()
	{
		Lock lock;
		CDL_CHECK(TryAcquireFromOtherThread(lock));

		lock.Acquire();
		CDL_CHECK_FALSE(TryAcquireFromOtherThread(lock));

		lock.Release();
		CDL_CHECK(TryAcquireFromOtherThread(lock));
	}

	template<typename Lock>
	void CheckWorksWithStandardLockGuards()
	{
		Lock lock;
		{
			std::scoped_lock guard(lock);
			CDL_CHECK_FALSE(TryAcquireFromOtherThread(lock));
		}
		CDL_CHECK(TryAcquireFromOtherThread(lock));

		std::unique_lock guard(lock, std::try_to_lock);
		CDL_CHECK(guard.owns_lock());
	}

	// A plain int incremented under the lock: any gap in mutual exclusion loses increments.
	template<typename Lock>
	void CheckMutualExclusionUnderContention()
	{
		constexpr int iterations = 50'000;
		const int threadCount = StressThreadCount();

		Lock lock;
		int64_t counter = 0;

		RunConcurrently(threadCount, [&](int) {
			for (int i = 0; i < iterations; ++i)
			{
				lock.Acquire();
				++counter;
				lock.Release();
			}
		});

		CDL_CHECK_EQ(counter, static_cast<int64_t>(threadCount) * iterations);
	}

}

CDL_TEST_CASE(SpinLock, TryAcquireExcludesOtherThreads, Unit)             { CheckTryAcquireExcludesOtherThreads<SpinLock>(); }
CDL_TEST_CASE(SpinLock, WorksWithStandardLockGuards, Unit)                { CheckWorksWithStandardLockGuards<SpinLock>(); }
CDL_TEST_CASE(SpinLock, MutualExclusionUnderContention, Stress)           { CheckMutualExclusionUnderContention<SpinLock>(); }

CDL_TEST_CASE(AdaptiveSpinLock, TryAcquireExcludesOtherThreads, Unit)     { CheckTryAcquireExcludesOtherThreads<AdaptiveSpinLock>(); }
CDL_TEST_CASE(AdaptiveSpinLock, WorksWithStandardLockGuards, Unit)        { CheckWorksWithStandardLockGuards<AdaptiveSpinLock>(); }
CDL_TEST_CASE(AdaptiveSpinLock, MutualExclusionUnderContention, Stress)   { CheckMutualExclusionUnderContention<AdaptiveSpinLock>(); }

CDL_TEST_CASE(ReentrantLock, TryAcquireExcludesOtherThreads, Unit)        { CheckTryAcquireExcludesOtherThreads<ReentrantLock>(); }
CDL_TEST_CASE(ReentrantLock, WorksWithStandardLockGuards, Unit)           { CheckWorksWithStandardLockGuards<ReentrantLock>(); }
CDL_TEST_CASE(ReentrantLock, MutualExclusionUnderContention, Stress)      { CheckMutualExclusionUnderContention<ReentrantLock>(); }

CDL_TEST_CASE(ReentrantLock, NestedAcquireHoldsUntilLastRelease, Unit)
{
	ReentrantLock lock;
	lock.Acquire();
	lock.Acquire();

	lock.Release();
	CDL_CHECK_FALSE(TryAcquireFromOtherThread(lock));

	lock.Release();
	CDL_CHECK(TryAcquireFromOtherThread(lock));
}

CDL_TEST_CASE(ReentrantLock, TryAcquireSucceedsOnOwningThread, Unit)
{
	ReentrantLock lock;
	lock.Acquire();
	CDL_CHECK(lock.TryAcquire());

	lock.Release();
	CDL_CHECK_FALSE(TryAcquireFromOtherThread(lock));

	lock.Release();
	CDL_CHECK(TryAcquireFromOtherThread(lock));
}

CDL_TEST_CASE(ScopedLock, HoldsForItsScope, Unit)
{
	SpinLock lock;
	{
		ScopedLock<SpinLock> guard(lock);
		CDL_CHECK_FALSE(TryAcquireFromOtherThread(lock));
	}
	CDL_CHECK(TryAcquireFromOtherThread(lock));
}

CDL_TEST_CASE(ScopedLock, NestsOverReentrantLock, Unit)
{
	ReentrantLock lock;
	{
		ScopedLock<ReentrantLock> outer(lock);
		{
			ScopedLock<ReentrantLock> inner(lock);
		}
		CDL_CHECK_FALSE(TryAcquireFromOtherThread(lock));
	}
	CDL_CHECK(TryAcquireFromOtherThread(lock));
}
