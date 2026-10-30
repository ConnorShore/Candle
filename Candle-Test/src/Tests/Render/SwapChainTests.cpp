// SwapChain on a real window: its images, the semaphores handing them to and from presentation, and frames driven
// through the same acquire → submit → present handoff RenderManager makes. Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/Vulkan/RenderDevice.h>
#include <Candle/Render/Vulkan/RenderInstance.h>
#include <Candle/Render/Vulkan/SwapChain.h>
#include <Candle/Render/Vulkan/SwapChainImage.h>

#include <format>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	// Construction order is dependency order: the swapchain's surface needs the window, so the window outlives it.
	struct TestSwapChain
	{
		WindowingBackend Backend;
		Window AppWindow{ TestWindowSpec() };
		RenderSpecification Spec = StrictRenderSpec();
		RenderInstance Instance{ TestInstanceSpec(Spec) };
		RenderDevice Device{ Instance, Spec };
		SwapChain Chain{ Instance, Device, AppWindow };
	};

	bool Presented(vk::Result result)
	{
		return result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR;
	}

	// One frame on slot 0, recording what Renderer does to its target minus the draw. Waits for the slot's last
	// submit first, as RenderManager does, so the pool and the slot's acquire semaphore are free to reuse.
	vk::Result PresentFrame(TestSwapChain& test, TestCommands& commands, uint64_t& retireValue)
	{
		constexpr uint32_t kFrameIndex = 0;

		test.Device.WaitFor(QueueType::Graphics, retireValue);
		commands.Pool.reset();

		const auto [acquired, imageIndex] = test.Chain.AquireNextImage(kFrameIndex);
		CDL_CHECK_MSG(Presented(acquired), std::format("acquire returned {}", vk::to_string(acquired)));

		SwapChainImage& target = test.Chain.GetImage(imageIndex);
		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		target.Transition({
			.Layout = vk::ImageLayout::eColorAttachmentOptimal,
			.Access = vk::AccessFlagBits2::eColorAttachmentWrite,
			.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput }, commands.Buffer);
		target.Transition({
			.Layout = vk::ImageLayout::ePresentSrcKHR,
			.Access = vk::AccessFlagBits2::eNone,
			.Stage = vk::PipelineStageFlagBits2::eNone }, commands.Buffer);
		commands.Buffer.end();

		const vk::CommandBufferSubmitInfo commandBufferInfo{ .commandBuffer = *commands.Buffer };
		const vk::SemaphoreSubmitInfo acquireWait = test.Chain.GetAcquireWait(kFrameIndex);
		const vk::SemaphoreSubmitInfo presentSignal = test.Chain.GetPresentSignal(imageIndex);
		retireValue = test.Device.Submit(QueueType::Graphics, {
			.CommandBuffers = { &commandBufferInfo, 1 },
			.Waits = { &acquireWait, 1 },
			.Signals = { &presentSignal, 1 } });

		return test.Chain.Present(imageIndex);
	}

}

// The extent comes from the surface, which on Windows is the window's size in pixels.
CDL_TEST_CASE(SwapChain, ImagesMatchTheWindowAndSurfaceFormat, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestSwapChain test;
		const glm::uvec2 size = test.AppWindow.GetSize();
		const ImageDesc& desc = test.Chain.GetImage(0).GetDesc();

		CDL_EXPECT_EQ(desc.Extent.width, size.x);
		CDL_EXPECT_EQ(desc.Extent.height, size.y);
		CDL_EXPECT_EQ(desc.Format, test.Chain.GetSurfaceFormat().format);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The acquire wait releases the first color write, the present signal covers the whole submission, and each image
// has its own present semaphore. Uses two images: the swapchain asks for three, and no desktop surface allows fewer than two.
CDL_TEST_CASE(SwapChain, HandoffSemaphoresCarryTheirStages, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestSwapChain test;
		const vk::SemaphoreSubmitInfo acquireWait = test.Chain.GetAcquireWait(0);
		const vk::SemaphoreSubmitInfo firstPresent = test.Chain.GetPresentSignal(0);
		const vk::SemaphoreSubmitInfo secondPresent = test.Chain.GetPresentSignal(1);

		CDL_EXPECT(acquireWait.stageMask == vk::PipelineStageFlagBits2::eColorAttachmentOutput);
		CDL_EXPECT(firstPresent.stageMask == vk::PipelineStageFlagBits2::eAllCommands);
		CDL_EXPECT(acquireWait.semaphore && firstPresent.semaphore && secondPresent.semaphore);
		CDL_EXPECT(firstPresent.semaphore != secondPresent.semaphore);
		CDL_EXPECT(acquireWait.semaphore != firstPresent.semaphore);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// More frames than images, so every image comes back from presentation and is acquired again.
CDL_TEST_CASE(SwapChain, PresentsMoreFramesThanItHasImages, Gpu)
{
	constexpr int kFrames = 8;

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestSwapChain test;
		TestCommands commands(test.Device);
		uint64_t retireValue = 0;

		for (int frame = 0; frame < kFrames; ++frame)
		{
			const vk::Result result = PresentFrame(test, commands, retireValue);
			CDL_EXPECT_MSG(Presented(result), std::format("frame {} presented {}", frame, vk::to_string(result)));
		}
		test.Device.WaitIdle();
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Recreating at the same size must leave a swapchain that still presents. The GPU is idled first, as a caller must.
CDL_TEST_CASE(SwapChain, PresentsAfterRecreate, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestSwapChain test;
		TestCommands commands(test.Device);
		uint64_t retireValue = 0;

		CDL_EXPECT(Presented(PresentFrame(test, commands, retireValue)));
		test.Device.WaitIdle();

		test.Chain.Recreate(test.AppWindow.GetSize());

		const vk::Result result = PresentFrame(test, commands, retireValue);
		CDL_EXPECT_MSG(Presented(result), std::format("presented {} after recreate", vk::to_string(result)));
		test.Device.WaitIdle();
	}
	ExpectNoVulkanDebugMessages(logs);
}

// RenderManager rebuilds whenever IsValid is false, so it must compare against the extent last asked for. The
// surface may pick its own extent (on Windows it always does), and a disagreeing one must not force a rebuild each frame.
CDL_TEST_CASE(SwapChain, IsValidOnlyForTheLastRequestedExtent, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestSwapChain test;
		const glm::uvec2 windowSize = test.AppWindow.GetSize();
		const glm::uvec2 otherSize = windowSize + glm::uvec2(100, 50);

		CDL_EXPECT(test.Chain.IsValid(windowSize));
		CDL_EXPECT_FALSE(test.Chain.IsValid(otherSize));

		// The window keeps its size, so the surface overrides this request
		test.Chain.Recreate(otherSize);

		CDL_EXPECT(test.Chain.IsValid(otherSize));
		CDL_EXPECT_FALSE(test.Chain.IsValid(windowSize));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Renderer sizes its depth target from GetExtent to cover the colour target's render area, so it must be what the images
// really are. The window keeps its size, so the surface overrides the request, as in IsValidOnlyForTheLastRequestedExtent.
CDL_TEST_CASE(SwapChain, GetExtentIsTheImagesExtentNotTheRequestedOne, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestSwapChain test;
		const glm::uvec2 windowSize = test.AppWindow.GetSize();
		test.Chain.Recreate(windowSize + glm::uvec2(100, 50));

		const vk::Extent2D extent = test.Chain.GetExtent();
		CDL_EXPECT(extent == test.Chain.GetImage(0).GetDesc().Extent);
		CDL_EXPECT_EQ(extent.width, windowSize.x);
		CDL_EXPECT_EQ(extent.height, windowSize.y);
	}
	ExpectNoVulkanDebugMessages(logs);
}
