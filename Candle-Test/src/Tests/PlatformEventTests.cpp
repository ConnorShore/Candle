// The backend-neutral side of PlatformEvent: how consumers dispatch on it. How a backend produces
// these events is tested per backend, under Tests/Platform/<Backend>/.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

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
