#include "TestFramework.h"
#include "TestHelpers.h"

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

CDL_TEST_CASE(ScopedPtr, DefaultIsNull, Unit)
{
	ScopedPtr<LifetimeCounter> ptr;
	CDL_CHECK(ptr.Get() == nullptr);
	CDL_CHECK_FALSE(ptr);
}

CDL_TEST_CASE(ScopedPtr, CreateForwardsArguments, Unit)
{
	LifetimeCounter::Reset();
	ScopedPtr<LifetimeCounter> ptr = ScopedPtr<LifetimeCounter>::Create(42);

	CDL_CHECK(ptr);
	CDL_CHECK_EQ(ptr->Value, 42);
	CDL_CHECK_EQ((*ptr).Value, 42);
	CDL_CHECK_EQ(LifetimeCounter::s_Constructed, 1);
}

CDL_TEST_CASE(ScopedPtr, DestructorDeletesExactlyOnce, Unit)
{
	LifetimeCounter::Reset();
	{
		ScopedPtr<LifetimeCounter> ptr = ScopedPtr<LifetimeCounter>::Create();
		CDL_CHECK_EQ(LifetimeCounter::Alive(), 1);
	}
	CDL_CHECK_EQ(LifetimeCounter::s_Destroyed, 1);
}

CDL_TEST_CASE(ScopedPtr, ResetDeletesOldAndTakesNew, Unit)
{
	LifetimeCounter::Reset();
	ScopedPtr<LifetimeCounter> ptr = ScopedPtr<LifetimeCounter>::Create(1);

	ptr.Reset(new LifetimeCounter(2));
	CDL_CHECK_EQ(LifetimeCounter::s_Destroyed, 1);
	CDL_CHECK_EQ(ptr->Value, 2);

	ptr.Reset();
	CDL_CHECK_EQ(LifetimeCounter::s_Destroyed, 2);
	CDL_CHECK_FALSE(ptr);
}

CDL_TEST_CASE(ScopedPtr, ReleaseGivesUpOwnershipWithoutDeleting, Unit)
{
	LifetimeCounter::Reset();
	ScopedPtr<LifetimeCounter> ptr = ScopedPtr<LifetimeCounter>::Create();

	LifetimeCounter* raw = ptr.Release();
	CDL_CHECK(raw != nullptr);
	CDL_CHECK_FALSE(ptr);
	CDL_CHECK_EQ(LifetimeCounter::s_Destroyed, 0);

	delete raw;
	CDL_CHECK_EQ(LifetimeCounter::s_Destroyed, 1);
}

CDL_TEST_CASE(ScopedPtr, MoveConstructTransfersOwnership, Unit)
{
	LifetimeCounter::Reset();
	ScopedPtr<LifetimeCounter> source = ScopedPtr<LifetimeCounter>::Create(7);
	LifetimeCounter* raw = source.Get();

	ScopedPtr<LifetimeCounter> destination(std::move(source));
	CDL_CHECK_FALSE(source);
	CDL_CHECK_EQ(destination.Get(), raw);
	CDL_CHECK_EQ(LifetimeCounter::Alive(), 1);
}

CDL_TEST_CASE(ScopedPtr, MoveAssignDeletesPreviousPointee, Unit)
{
	LifetimeCounter::Reset();
	ScopedPtr<LifetimeCounter> first = ScopedPtr<LifetimeCounter>::Create(1);
	ScopedPtr<LifetimeCounter> second = ScopedPtr<LifetimeCounter>::Create(2);

	first = std::move(second);
	CDL_CHECK_EQ(LifetimeCounter::s_Destroyed, 1);
	CDL_CHECK_EQ(first->Value, 2);
	CDL_CHECK_FALSE(second);
}

CDL_TEST_CASE(ScopedPtr, SelfMoveAssignKeepsPointee, Unit)
{
	LifetimeCounter::Reset();
	ScopedPtr<LifetimeCounter> ptr = ScopedPtr<LifetimeCounter>::Create(5);

	// Through a reference so the compiler doesn't warn about the deliberate self-move.
	ScopedPtr<LifetimeCounter>& alias = ptr;
	ptr = std::move(alias);

	CDL_CHECK(ptr);
	CDL_CHECK_EQ(ptr->Value, 5);
	CDL_CHECK_EQ(LifetimeCounter::s_Destroyed, 0);
}

CDL_TEST_CASE(ScopedPtr, DerivedToBaseRunsDerivedDestructor, Unit)
{
	LifetimeCounter::Reset();
	DerivedLifetimeCounter::s_DerivedDestroyed = 0;
	{
		ScopedPtr<LifetimeCounter> base = ScopedPtr<DerivedLifetimeCounter>::Create(3);
		CDL_CHECK_EQ(base->Value, 3);

		ScopedPtr<LifetimeCounter> assigned;
		assigned = ScopedPtr<DerivedLifetimeCounter>::Create(4);
		CDL_CHECK_EQ(assigned->Value, 4);
	}
	CDL_CHECK_EQ(DerivedLifetimeCounter::s_DerivedDestroyed, 2);
	CDL_CHECK_EQ(LifetimeCounter::s_Destroyed, 2);
}

CDL_TEST_CASE(ScopedPtr, StaticPointerCastIsNonOwning, Unit)
{
	LifetimeCounter::Reset();
	ScopedPtr<LifetimeCounter> base = ScopedPtr<DerivedLifetimeCounter>::Create(9);

	DerivedLifetimeCounter* derived = StaticPointerCast<DerivedLifetimeCounter>(base);
	CDL_CHECK_EQ(static_cast<LifetimeCounter*>(derived), base.Get());
	CDL_CHECK(base); // still owns it
	CDL_CHECK_EQ(LifetimeCounter::s_Destroyed, 0);
}

CDL_TEST_CASE(ScopedPtr, EqualityComparesPointees, Unit)
{
	ScopedPtr<LifetimeCounter> a = ScopedPtr<LifetimeCounter>::Create();
	ScopedPtr<LifetimeCounter> b = ScopedPtr<LifetimeCounter>::Create();
	ScopedPtr<LifetimeCounter> null1, null2;

	CDL_CHECK(a == a);
	CDL_CHECK_FALSE(a == b);
	CDL_CHECK(null1 == null2);
}
