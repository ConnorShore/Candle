// Window's contract through the backend-neutral API only, so any windowing backend has to pass these.
// They open real OS windows: the Display type, skipped by --filter=unit and wherever there is no display.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Display;

namespace {

	void DiscardPendingEvents()
	{
		std::vector<PlatformEvent> events;
		Platform::PumpEvents(events);
	}

	// Window operations can land a few pumps later, so poll until a matching event arrives or time runs out.
	template<typename T, typename Pred>
	bool WaitForEvent(Pred matches, double timeoutMs = 2000.0)
	{
		std::vector<PlatformEvent> events;
		const Timer timer;
		do
		{
			Platform::PumpEvents(events);
			for (const PlatformEvent& evt : events)
			{
				if (const T* e = std::get_if<T>(&evt); e && matches(*e))
					return true;
			}
			Platform::SleepCurrentThread(5);
		} while (timer.ElapsedMilliseconds() < timeoutMs);
		return false;
	}

}

// Pixel size depends on display scaling, but the spec's aspect ratio has to survive it.
CDL_TEST_CASE(Window, SizeIsInPixelsWithTheSpecsAspectRatio, Display)
{
	WindowingBackend backend;
	const Window window(TestWindowSpec(640, 480));

	const glm::uvec2 size = window.GetSize();
	CDL_NOTE(std::format("640x480 spec is {}x{} pixels", size.x, size.y));
	CDL_CHECK(size.x > 0 && size.y > 0);
	CDL_EXPECT_NEAR(static_cast<double>(size.x) / size.y, 640.0 / 480.0, 0.01);
	CDL_EXPECT_EQ(window.GetWidth(), size.x);
	CDL_EXPECT_EQ(window.GetHeight(), size.y);
}

// The render thread will size the swapchain from WindowResized while the main thread calls GetSize, so they must agree.
CDL_TEST_CASE(Window, ResizeEventReportsTheSameSizeAsGetSize, Display)
{
	WindowingBackend backend;
	Window window(TestWindowSpec(640, 480));
	DiscardPendingEvents();

	const glm::uvec2 before = window.GetSize();
	window.Resize(800, 600);

	const bool reported = WaitForEvent<WindowResized>([&](const WindowResized& e) {
		return e.Width == window.GetSize().x && e.Height == window.GetSize().y;
	});

	const glm::uvec2 after = window.GetSize();
	CDL_EXPECT(after != before);
	CDL_EXPECT_MSG(reported, std::format("no WindowResized matching GetSize() {}x{}", after.x, after.y));
}

CDL_TEST_CASE(Window, MinimizeAndRestoreAreReported, Display)
{
	WindowingBackend backend;
	Window window(TestWindowSpec());
	DiscardPendingEvents();

	window.Minimize();
	CDL_CHECK_MSG(WaitForEvent<WindowMinimized>([](const WindowMinimized& e) { return e.Minimized; }), "no minimize event");

	window.Restore();
	CDL_CHECK_MSG(WaitForEvent<WindowMinimized>([](const WindowMinimized& e) { return !e.Minimized; }), "no restore event");
}
