// SwapChain on a real window: its images, the semaphores handing them to and from presentation, and frames driven
// through the same acquire → submit → present handoff RenderManager makes. Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/Image.h>
#include <Candle/Render/RenderDevice.h>
#include <Candle/Render/RenderInstance.h>
#include <Candle/Render/SwapChain.h>

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

		Image& target = test.Chain.GetImage(imageIndex);
		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		target.Transition({
			.Layout = vk::ImageLayout::eColorAttachmentOptimal,
			.AccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.StageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.AspectMask = vk::ImageAspectFlagBits::eColor }, commands.Buffer);
		target.Transition({
			.Layout = vk::ImageLayout::ePresentSrcKHR,
			.AccessMask = vk::AccessFlagBits2::eNone,
			.StageMask = vk::PipelineStageFlagBits2::eNone,
			.AspectMask = vk::ImageAspectFlagBits::eColor }, commands.Buffer);
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
