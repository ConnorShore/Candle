// Blackboard is how render modules hand each other their outputs for the frame, one value per type. Get and TryGet find a value by
// its type alone, Set replaces whatever that type held, and Clear, which FrameGraph::Reset calls, drops the lot. Values live in
// the arena it's given, the frame graph's own in the engine, so each test hands it one.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Render/FrameGraph/Blackboard.h>

#include <cstdint>
#include <type_traits>
#include <utility>

using namespace Candle;
using Candle::Test::Type::Unit;

namespace {

	constexpr size_t kArenaCapacity = 4 * 1024;

	struct SceneData
	{
		uint32_t Width{ 0 };
	};

	// Laid out exactly like SceneData, so only the type tells them apart
	struct GuiData
	{
		uint32_t Width{ 0 };
	};

	template<uint32_t N>
	struct Numbered
	{
		uint32_t Value{ N };
	};

	template<uint32_t... N>
	void SetEach(Blackboard& blackboard, std::integer_sequence<uint32_t, N...>)
	{
		(blackboard.Set<Numbered<N>>(), ...);
	}

}

// Whoever only holds a const blackboard mustn't be able to change what's in it
CDL_STATIC_ASSERT(std::is_same_v<decltype(std::declval<const Blackboard&>().Get<SceneData>()), const SceneData&>);
CDL_STATIC_ASSERT(std::is_same_v<decltype(std::declval<const Blackboard&>().TryGet<SceneData>()), const SceneData*>);

CDL_TEST_CASE(Blackboard, GetReturnsWhatSetStored, Unit)
{
	ArenaAllocator arena(kArenaCapacity);
	Blackboard blackboard(arena);
	blackboard.Set<SceneData>(SceneData{ .Width = 7 });

	CDL_EXPECT_EQ(blackboard.Get<SceneData>().Width, 7u);
	CDL_CHECK(blackboard.TryGet<SceneData>() != nullptr);
	CDL_EXPECT_EQ(blackboard.TryGet<SceneData>()->Width, 7u);
}

CDL_TEST_CASE(Blackboard, SetHandsBackTheStoredValue, Unit)
{
	ArenaAllocator arena(kArenaCapacity);
	Blackboard blackboard(arena);
	SceneData& stored = blackboard.Set<SceneData>();
	stored.Width = 9;

	CDL_EXPECT(&stored == &blackboard.Get<SceneData>());
	CDL_EXPECT_EQ(blackboard.Get<SceneData>().Width, 9u);
}

CDL_TEST_CASE(Blackboard, SettingATypeAgainReplacesItsValue, Unit)
{
	ArenaAllocator arena(kArenaCapacity);
	Blackboard blackboard(arena);
	blackboard.Set<SceneData>(SceneData{ .Width = 1 });
	blackboard.Set<SceneData>(SceneData{ .Width = 2 });

	CDL_EXPECT_EQ(blackboard.Get<SceneData>().Width, 2u);
}

// A module may hold the first Set's reference while a later one replaces the value, so the replacement goes where it was
CDL_TEST_CASE(Blackboard, ReplacingAValueKeepsItsAddress, Unit)
{
	ArenaAllocator arena(kArenaCapacity);
	Blackboard blackboard(arena);
	const SceneData& first = blackboard.Set<SceneData>(SceneData{ .Width = 1 });
	const SceneData& second = blackboard.Set<SceneData>(SceneData{ .Width = 2 });

	CDL_EXPECT(&first == &second);
	CDL_EXPECT_EQ(first.Width, 2u);
}

// How GuiModule tells a frame that drew the preview from one that didn't
CDL_TEST_CASE(Blackboard, TryGetIsNullForATypeNeverSet, Unit)
{
	ArenaAllocator arena(kArenaCapacity);
	Blackboard blackboard(arena);
	blackboard.Set<SceneData>();

	CDL_EXPECT(blackboard.TryGet<GuiData>() == nullptr);
}

CDL_TEST_CASE(Blackboard, TypesWithTheSameLayoutAreKeptApart, Unit)
{
	ArenaAllocator arena(kArenaCapacity);
	Blackboard blackboard(arena);
	blackboard.Set<SceneData>(SceneData{ .Width = 1 });
	blackboard.Set<GuiData>(GuiData{ .Width = 2 });

	CDL_EXPECT_EQ(blackboard.Get<SceneData>().Width, 1u);
	CDL_EXPECT_EQ(blackboard.Get<GuiData>().Width, 2u);
}

// Get hands out references, so a value has to stay where it is while later modules Set theirs and the type list grows
CDL_TEST_CASE(Blackboard, ValuesStayPutAsMoreTypesAreSet, Unit)
{
	ArenaAllocator arena(kArenaCapacity);
	Blackboard blackboard(arena);
	const SceneData* scene = &blackboard.Set<SceneData>(SceneData{ .Width = 3 });

	SetEach(blackboard, std::make_integer_sequence<uint32_t, 64>{});

	CDL_EXPECT(&blackboard.Get<SceneData>() == scene);
	CDL_EXPECT_EQ(scene->Width, 3u);
	CDL_EXPECT_EQ(blackboard.Get<Numbered<63>>().Value, 63u);
}

CDL_TEST_CASE(Blackboard, ClearDropsEveryValue, Unit)
{
	ArenaAllocator arena(kArenaCapacity);
	Blackboard blackboard(arena);
	blackboard.Set<SceneData>(SceneData{ .Width = 1 });
	blackboard.Set<GuiData>(GuiData{ .Width = 2 });

	blackboard.Clear();
	CDL_EXPECT(blackboard.TryGet<SceneData>() == nullptr);
	CDL_EXPECT(blackboard.TryGet<GuiData>() == nullptr);

	blackboard.Set<SceneData>(SceneData{ .Width = 3 });
	CDL_EXPECT_EQ(blackboard.Get<SceneData>().Width, 3u);
}

CDL_TEST_CASE(Blackboard, ConstBlackboardFindsTheSameValues, Unit)
{
	ArenaAllocator arena(kArenaCapacity);
	Blackboard blackboard(arena);
	blackboard.Set<SceneData>(SceneData{ .Width = 4 });

	const Blackboard& readOnly = blackboard;
	CDL_EXPECT_EQ(readOnly.Get<SceneData>().Width, 4u);
	CDL_EXPECT(&readOnly.Get<SceneData>() == &blackboard.Get<SceneData>());
	CDL_EXPECT(readOnly.TryGet<SceneData>() == &blackboard.Get<SceneData>());
	CDL_EXPECT(readOnly.TryGet<GuiData>() == nullptr);
}
