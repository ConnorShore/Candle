// The backend-neutral side of PlatformEvent: how consumers dispatch on it. How a backend produces
// these events is tested per backend, under Tests/Platform/<Backend>/.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <set>
#include <variant>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

// Declared constexpr, so usable where the event is known at compile time.
CDL_STATIC_ASSERT(IsKeyEvent(InputEvent{ KeyPressedEvent{ KeyCode::A } }) && !IsMouseEvent(InputEvent{ KeyPressedEvent{ KeyCode::A } }));
CDL_STATIC_ASSERT(IsMouseEvent(InputEvent{ MouseButtonPressedEvent{ MouseButton::Left } }) && !IsKeyEvent(InputEvent{ MouseButtonPressedEvent{ MouseButton::Left } }));

// GuiSystem::WantsInput routes on IsKeyEvent and IsMouseEvent, so a new InputEvent alternative must be classified
// deliberately; this fails the build until kKeyEvents or kMouseEvents covers it.
CDL_STATIC_ASSERT(std::variant_size_v<InputEvent> == 6, "A new InputEvent alternative needs a case in IsKeyEvent, IsMouseEvent and the tests");

// Every alternative is exactly one of the two, so the GUI capturing one device never swallows the other's events.
CDL_TEST_CASE(PlatformEvents, EveryInputEventIsEitherAKeyOrAMouseEvent, Unit)
{
	std::set<size_t> covered;
	for (const InputEvent& event : kKeyEvents)
	{
		CDL_EXPECT_MSG(IsKeyEvent(event), std::format("alternative {}", event.index()));
		CDL_EXPECT_MSG(!IsMouseEvent(event), std::format("alternative {}", event.index()));
		covered.insert(event.index());
	}
	for (const InputEvent& event : kMouseEvents)
	{
		CDL_EXPECT_MSG(IsMouseEvent(event), std::format("alternative {}", event.index()));
		CDL_EXPECT_MSG(!IsKeyEvent(event), std::format("alternative {}", event.index()));
		covered.insert(event.index());
	}

	CDL_EXPECT_EQ(covered.size(), std::variant_size_v<InputEvent>);
}

// The dispatch Application uses: an exact-type handler wins over the generic catch-all.
CDL_TEST_CASE(PlatformEvents, OverloadedVisitDispatchesByType, Unit)
{
	const std::vector<PlatformEvent> events = { WindowResized{ .WindowID = 1, .Width = 3, .Height = 4 }, QuitRequested{}, WindowMinimized{ .WindowID = 1, .Minimized = true } };

	int quits = 0, resizes = 0, others = 0;
	uint32_t area = 0;
	for (const PlatformEvent& evt : events)
	{
		std::visit(Overloaded{
			[&](const QuitRequested&) { ++quits; },
			[&](const WindowResized& e) { ++resizes; area = e.Width * e.Height; },
			[&](const auto&) { ++others; },
		}, evt);
	}

	CDL_EXPECT_EQ(quits, 1);
	CDL_EXPECT_EQ(resizes, 1);
	CDL_EXPECT_EQ(area, 12u);
	CDL_EXPECT_EQ(others, 1);
}
