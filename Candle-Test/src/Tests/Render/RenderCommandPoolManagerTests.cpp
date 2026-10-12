// RenderCommandPoolManager: one pool per frame slot per worker, so recording needs no locks. Validation's
// thread-safety checks fail these if two threads ever share a pool, and its state checks fail them if a pool is
// reset while its buffers are still pending.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/RenderCommandPoolManager.h>
#include <Candle/Render/RenderConstants.h>
#include <Candle/Render/RenderDevice.h>

#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

// One worker's pool through several frames: acquire, record, submit, wait, reset.
CDL_TEST_CASE(RenderCommandPoolManager, ThreadCommandsResetForReuseOnceRetired, Gpu)
{
	constexpr int kFrames = 3;

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		ThreadFrameCommands commands(gpu.Device, gpu.Device.GetQueueFamily(QueueType::Graphics));

		for (int frame = 0; frame < kFrames; ++frame)
		{
			vk::raii::CommandBuffer& buffer = commands.AquireCommandBuffer();
			buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
			buffer.end();

			SubmitAndWait(gpu.Device, buffer);
			commands.Reset();
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Every worker records into its own pool at the same moment, then one submit takes all their buffers. The second
// round on each slot only passes if BeginFrame left that slot's pools reusable.
CDL_TEST_CASE(RenderCommandPoolManager, EveryThreadRecordsIntoItsOwnPoolAtOnce, Gpu)
{
	constexpr uint32_t kRounds = 2 * kMaxFramesInFlight;

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const int threadCount = StressThreadCount();
		RenderCommandPoolManager pools(gpu.Device, static_cast<uint32_t>(threadCount));

		for (uint32_t round = 0; round < kRounds; ++round)
		{
			const uint32_t frameIndex = round % kMaxFramesInFlight;
			pools.BeginFrame(frameIndex);

			// Copied out as handles straight away; each thread writes only its own element
			std::vector<vk::CommandBufferSubmitInfo> recorded(threadCount);
			RunConcurrently(threadCount, [&](int thread)
			{
				vk::raii::CommandBuffer& buffer = pools.GetGraphicsCommands(frameIndex, static_cast<uint32_t>(thread)).AquireCommandBuffer();
				buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
				buffer.end();
				recorded[thread] = { .commandBuffer = *buffer };
			});

			gpu.Device.WaitFor(QueueType::Graphics, gpu.Device.Submit(QueueType::Graphics, { .CommandBuffers = recorded }));
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}
