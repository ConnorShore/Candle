#include "TestFramework.h"
#include "TestHelpers.h"

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Stress;

CDL_TEST_CASE(SharedPtr, DefaultAndNullptrAreNull, Unit)
{
	SharedPtr<SharedCounter> defaulted;
	SharedPtr<SharedCounter> fromNull = nullptr;

	CDL_CHECK_FALSE(defaulted);
	CDL_CHECK_FALSE(fromNull);
	CDL_CHECK(defaulted == fromNull);
}

CDL_TEST_CASE(SharedPtr, CreateStartsAtRefCountOne, Unit)
{
	SharedPtr<SharedCounter> ptr = SharedPtr<SharedCounter>::Create(11);
	CDL_CHECK_EQ(ptr->Value, 11);
	CDL_CHECK_EQ(ptr->GetRefCount(), 1u);
}

CDL_TEST_CASE(SharedPtr, CopyIncrementsAndMoveDoesNot, Unit)
{
	SharedPtr<SharedCounter> original = SharedPtr<SharedCounter>::Create();

	SharedPtr<SharedCounter> copy = original;
	CDL_CHECK_EQ(original->GetRefCount(), 2u);
	CDL_CHECK(copy == original);

	SharedPtr<SharedCounter> moved = std::move(copy);
	CDL_CHECK_FALSE(copy);
	CDL_CHECK_EQ(original->GetRefCount(), 2u);
}

CDL_TEST_CASE(SharedPtr, LastReleaseDeletesExactlyOnce, Unit)
{
	SharedCounter::Reset();
	{
		SharedPtr<SharedCounter> a = SharedPtr<SharedCounter>::Create();
		{
			SharedPtr<SharedCounter> b = a;
		}
		CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 0);
		CDL_CHECK_EQ(a->GetRefCount(), 1u);
	}
	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 1);
}

CDL_TEST_CASE(SharedPtr, ResetReleasesOnlyThisReference, Unit)
{
	SharedCounter::Reset();
	SharedPtr<SharedCounter> a = SharedPtr<SharedCounter>::Create();
	SharedPtr<SharedCounter> b = a;

	b.Reset();
	CDL_CHECK_FALSE(b);
	CDL_CHECK_EQ(a->GetRefCount(), 1u);
	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 0);

	a.Reset();
	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 1);
}

CDL_TEST_CASE(SharedPtr, AssignOverLivePointerReleasesOld, Unit)
{
	SharedCounter::Reset();
	SharedPtr<SharedCounter> a = SharedPtr<SharedCounter>::Create(1);
	SharedPtr<SharedCounter> b = SharedPtr<SharedCounter>::Create(2);

	a = b;
	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 1);
	CDL_CHECK_EQ(a->Value, 2);
	CDL_CHECK_EQ(b->GetRefCount(), 2u);

	SharedPtr<SharedCounter> c = SharedPtr<SharedCounter>::Create(3);
	a = std::move(c);
	CDL_CHECK_EQ(a->Value, 3);
	CDL_CHECK_EQ(b->GetRefCount(), 1u);
	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 1);
}

CDL_TEST_CASE(SharedPtr, SelfAssignKeepsObjectAlive, Unit)
{
	SharedCounter::Reset();
	SharedPtr<SharedCounter> ptr = SharedPtr<SharedCounter>::Create(8);

	// Through a reference so the compiler doesn't warn about the deliberate self-assignment.
	SharedPtr<SharedCounter>& alias = ptr;
	ptr = alias;
	CDL_CHECK_EQ(ptr->GetRefCount(), 1u);

	ptr = std::move(alias);
	CDL_CHECK(ptr);
	CDL_CHECK_EQ(ptr->Value, 8);
	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 0);
}

CDL_TEST_CASE(SharedPtr, DerivedConvertsToBaseAndSharesCount, Unit)
{
	SharedCounter::Reset();
	SharedPtr<DerivedSharedCounter> derived = SharedPtr<DerivedSharedCounter>::Create(4);

	SharedPtr<SharedCounter> base = derived;
	CDL_CHECK_EQ(derived->GetRefCount(), 2u);

	SharedPtr<SharedCounter> assigned;
	assigned = derived;
	CDL_CHECK_EQ(derived->GetRefCount(), 3u);

	derived.Reset();
	assigned.Reset();
	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 0);
	base.Reset();
	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 1);
}

CDL_TEST_CASE(SharedPtr, StaticPointerCastSharesOwnership, Unit)
{
	SharedPtr<SharedCounter> base = SharedPtr<DerivedSharedCounter>::Create(6);

	SharedPtr<DerivedSharedCounter> derived = StaticPointerCast<DerivedSharedCounter>(base);
	CDL_CHECK_EQ(derived->Value, 6);
	CDL_CHECK_EQ(base->GetRefCount(), 2u);
}

CDL_TEST_CASE(SharedPtr, DynamicPointerCastReturnsNullOnMismatch, Unit)
{
	SharedPtr<SharedCounter> base = SharedPtr<DerivedSharedCounter>::Create();

	SharedPtr<DerivedSharedCounter> hit = DynamicPointerCast<DerivedSharedCounter>(base);
	SharedPtr<UnrelatedSharedCounter> miss = DynamicPointerCast<UnrelatedSharedCounter>(base);

	CDL_CHECK(hit);
	CDL_CHECK_FALSE(miss);
	CDL_CHECK_EQ(base->GetRefCount(), 2u);
}

CDL_TEST_CASE(SharedPtr, TwoPointersFromOneRawShareTheIntrusiveCount, Unit)
{
	// Unlike std::shared_ptr, the count lives in the object, so re-wrapping a raw pointer is safe.
	SharedCounter::Reset();
	SharedCounter* raw = new SharedCounter();
	{
		SharedPtr<SharedCounter> first(raw);
		SharedPtr<SharedCounter> second(raw);
		CDL_CHECK_EQ(raw->GetRefCount(), 2u);
	}
	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 1);
}

CDL_TEST_CASE(SharedPtr, ConcurrentCopiesLeaveCountBalanced, Stress)
{
	SharedCounter::Reset();
	SharedPtr<SharedCounter> shared = SharedPtr<SharedCounter>::Create();

	constexpr int iterations = 20'000;
	RunConcurrently(StressThreadCount(), [&](int) {
		for (int i = 0; i < iterations; ++i)
		{
			SharedPtr<SharedCounter> copy = shared;
			SharedPtr<SharedCounter> another;
			another = copy;
		}
	});

	CDL_CHECK_EQ(shared->GetRefCount(), 1u);
	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 0);
}

CDL_TEST_CASE(SharedPtr, ConcurrentLastReleaseDeletesEachObjectOnce, Stress)
{
	// Every thread holds a reference to every object, then all drop them at once, so for each object
	// exactly one thread sees the count hit zero. A double delete or a missed delete shows in the tally.
	SharedCounter::Reset();
	constexpr int objectCount = 2'000;
	const int threadCount = StressThreadCount();

	std::vector<std::vector<SharedPtr<SharedCounter>>> perThread(threadCount);
	{
		std::vector<SharedPtr<SharedCounter>> originals;
		for (int i = 0; i < objectCount; ++i)
			originals.push_back(SharedPtr<SharedCounter>::Create(i));

		for (auto& refs : perThread)
			refs = originals;
	}
	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), 0);

	RunConcurrently(threadCount, [&](int thread) {
		auto& refs = perThread[thread];
		// Alternate release order so threads collide on the same objects from both ends.
		if (thread % 2)
			while (!refs.empty()) refs.pop_back();
		else
			for (auto& ref : refs) ref.Reset();
	});

	CDL_CHECK_EQ(SharedCounter::s_Destroyed.load(), objectCount);
}
