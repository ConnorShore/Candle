// DeletionQueue with the GPU timeline faked by hand, holding real buffers so VMA's live count shows exactly when one is
// destroyed. Nothing is submitted, so no buffer is ever in use. Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/DeletionQueue.h>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	GPUBuffer AnyBuffer(GPUAllocator& allocator) { return CreateHostBuffer(allocator, 64); }

}

// The queue only knows the values it is told, so a resource lives until a drain reports its value reached.
CDL_TEST_CASE(DeletionQueue, HoldsAResourceUntilItsRetireValueIsReached, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		DeletionQueue deletions;
		deletions.Enqueue(QueueType::Graphics, 5, AnyBuffer(gpu.Allocator));
		CDL_CHECK_EQ(LiveAllocations(gpu.Allocator), 1u);

		deletions.Drain(QueueType::Graphics, 4);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);

		deletions.Drain(QueueType::Graphics, 5);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Retire values rise, so a drain frees from the front, equal values included, and stops at the first one not reached.
CDL_TEST_CASE(DeletionQueue, DrainsInRetireOrderUpToTheCompletedValue, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		DeletionQueue deletions;
		for (uint64_t retireValue : { 1, 2, 2, 3 })
			deletions.Enqueue(QueueType::Graphics, retireValue, AnyBuffer(gpu.Allocator));

		deletions.Drain(QueueType::Graphics, 2);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);

		deletions.Drain(QueueType::Graphics, 3);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Each queue has its own timeline, so a value reached on one says nothing about the other.
CDL_TEST_CASE(DeletionQueue, QueuesDrainIndependently, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		DeletionQueue deletions;
		deletions.Enqueue(QueueType::Graphics, 1, AnyBuffer(gpu.Allocator));
		deletions.Enqueue(QueueType::Transfer, 1, AnyBuffer(gpu.Allocator));

		deletions.Drain(QueueType::Transfer, 10);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);

		deletions.Drain(QueueType::Graphics, 1);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Shutdown, once the GPU is idle: DrainAll frees every queue's resources whatever their values, as destruction does.
CDL_TEST_CASE(DeletionQueue, DrainAllAndDestructionFreeEverything, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		{
			DeletionQueue deletions;
			deletions.Enqueue(QueueType::Graphics, 100, AnyBuffer(gpu.Allocator));
			deletions.Enqueue(QueueType::Transfer, 100, AnyBuffer(gpu.Allocator));

			deletions.DrainAll();
			CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);

			deletions.Enqueue(QueueType::Graphics, 200, AnyBuffer(gpu.Allocator));
		}
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}
