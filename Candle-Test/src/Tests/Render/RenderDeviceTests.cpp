// RenderDevice: queue-family selection as a pure function over synthetic driver layouts (Unit), and a real
// device created and destroyed without validation output (Gpu). The Gpu timeline tests submit empty batches,
// which still signal the queue's timeline, so they check Submit and WaitFor without recording any commands.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/RenderDevice.h>
#include <Candle/Render/RenderInstance.h>

#include <algorithm>
#include <atomic>
#include <format>
#include <functional>
#include <initializer_list>
#include <map>
#include <numeric>
#include <optional>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;
using Candle::Test::Type::Unit;

namespace {

	using enum vk::QueueFlagBits;

	// One family per entry, in the order a driver reports them.
	std::vector<vk::QueueFamilyProperties> Families(std::initializer_list<vk::QueueFlags> flags)
	{
		std::vector<vk::QueueFamilyProperties> families;
		for (vk::QueueFlags familyFlags : flags)
			families.push_back({ .queueFlags = familyFlags, .queueCount = 1 });
		return families;
	}

	// Every family presents, as on Windows desktop drivers; the present tests pass their own support.
	std::optional<QueueFamilyIndices> FindAllPresenting(std::span<const vk::QueueFamilyProperties> families)
	{
		return FindQueueFamilies(families, std::vector<vk::Bool32>(families.size(), VK_TRUE));
	}

}

// NVIDIA's layout: a pure DMA family ahead of the compute and video families that also report transfer.
CDL_TEST_CASE(RenderDevice, PrefersADedicatedTransferFamily, Unit)
{
	const auto families = Families({
		eGraphics | eCompute | eTransfer | eSparseBinding,
		eTransfer | eSparseBinding,
		eCompute | eTransfer | eSparseBinding,
		eTransfer | eSparseBinding | eVideoDecodeKHR });

	const std::optional<QueueFamilyIndices> indices = FindAllPresenting(families);
	CDL_CHECK(indices.has_value());
	CDL_EXPECT_EQ(indices->Graphics, 0u);
	CDL_EXPECT_EQ(indices->Transfer, 1u);
}

// AMD's layout: the async compute family comes before the DMA family and must not be taken for transfer.
CDL_TEST_CASE(RenderDevice, SkipsComputeFamiliesForTransfer, Unit)
{
	const auto families = Families({
		eGraphics | eCompute | eTransfer | eSparseBinding,
		eCompute | eTransfer | eSparseBinding,
		eTransfer | eSparseBinding });

	const std::optional<QueueFamilyIndices> indices = FindAllPresenting(families);
	CDL_CHECK(indices.has_value());
	CDL_EXPECT_EQ(indices->Transfer, 2u);
}

// Video families report transfer too, but they are a different engine and usually expose a single queue.
CDL_TEST_CASE(RenderDevice, SkipsVideoFamiliesForTransfer, Unit)
{
	const auto families = Families({
		eGraphics | eCompute | eTransfer,
		eTransfer | eVideoDecodeKHR,
		eTransfer });

	const std::optional<QueueFamilyIndices> indices = FindAllPresenting(families);
	CDL_CHECK(indices.has_value());
	CDL_EXPECT_EQ(indices->Transfer, 2u);
}

// Older Intel iGPUs expose one family for everything, so Transfer has to alias Graphics.
CDL_TEST_CASE(RenderDevice, TransferAliasesGraphicsWithoutADedicatedFamily, Unit)
{
	const std::optional<QueueFamilyIndices> single = FindAllPresenting(Families({ eGraphics | eCompute | eTransfer }));
	CDL_CHECK(single.has_value());
	CDL_EXPECT_EQ(single->Graphics, 0u);
	CDL_EXPECT_EQ(single->Transfer, 0u);

	const std::optional<QueueFamilyIndices> withCompute = FindAllPresenting(Families({ eGraphics | eCompute | eTransfer, eCompute | eTransfer }));
	CDL_CHECK(withCompute.has_value());
	CDL_EXPECT_EQ(withCompute->Transfer, withCompute->Graphics);
}

// The spec makes reporting the transfer bit optional on graphics and compute families, since both imply it.
CDL_TEST_CASE(RenderDevice, GraphicsFamilyNeedNotReportTransfer, Unit)
{
	const std::optional<QueueFamilyIndices> indices = FindAllPresenting(Families({ eGraphics | eCompute }));
	CDL_CHECK(indices.has_value());
	CDL_EXPECT_EQ(indices->Graphics, 0u);
	CDL_EXPECT_EQ(indices->Transfer, 0u);
}

CDL_TEST_CASE(RenderDevice, FindsAGraphicsFamilyThatIsNotFirst, Unit)
{
	const std::optional<QueueFamilyIndices> indices = FindAllPresenting(Families({ eCompute | eTransfer, eTransfer, eGraphics | eCompute | eTransfer }));
	CDL_CHECK(indices.has_value());
	CDL_EXPECT_EQ(indices->Graphics, 2u);
	CDL_EXPECT_EQ(indices->Transfer, 1u);
}

// Graphics and compute must share a family; split across two, the device is unsuitable.
CDL_TEST_CASE(RenderDevice, RejectsADeviceWithoutAGraphicsComputeFamily, Unit)
{
	CDL_EXPECT_FALSE(FindAllPresenting(Families({ eGraphics | eTransfer, eCompute | eTransfer })).has_value());
	CDL_EXPECT_FALSE(FindAllPresenting({}).has_value());
}

// Presenting gates Graphics only; the DMA family is still taken for transfer though it cannot present.
CDL_TEST_CASE(RenderDevice, SkipsAGraphicsFamilyThatCannotPresent, Unit)
{
	const auto families = Families({ eGraphics | eCompute | eTransfer, eTransfer, eGraphics | eCompute | eTransfer });
	const std::vector<vk::Bool32> presentSupport = { VK_FALSE, VK_FALSE, VK_TRUE };

	const std::optional<QueueFamilyIndices> indices = FindQueueFamilies(families, presentSupport);
	CDL_CHECK(indices.has_value());
	CDL_EXPECT_EQ(indices->Graphics, 2u);
	CDL_EXPECT_EQ(indices->Transfer, 1u);
}

// A present-capable family that can't do graphics doesn't help; Candle has no separate present queue.
CDL_TEST_CASE(RenderDevice, RejectsADeviceWhoseGraphicsFamilyCannotPresent, Unit)
{
	const auto families = Families({ eGraphics | eCompute | eTransfer, eCompute | eTransfer });
	const std::vector<vk::Bool32> presentSupport = { VK_FALSE, VK_TRUE };

	CDL_EXPECT_FALSE(FindQueueFamilies(families, presentSupport).has_value());
}

// Selection, logical device and queue retrieval on whatever GPU the machine picks, checked by validation.
CDL_TEST_CASE(RenderDevice, CreatesAndDestroysWithoutValidationMessages, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		WindowingBackend backend;
		const RenderSpecification spec = StrictRenderSpec();
		RenderInstance instance(TestInstanceSpec(spec));
		RenderDevice device(instance, spec);

		CDL_EXPECT(*device.GetDevice());
		CDL_NOTE(std::format("graphics family {}, transfer family {}",
			device.GetQueueFamily(QueueType::Graphics), device.GetQueueFamily(QueueType::Transfer)));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A fresh timeline starts at zero, so values count a queue's submissions from one and waiting for zero never blocks.
CDL_TEST_CASE(RenderDevice, SubmitReturnsConsecutiveTimelineValues, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		gpu.Device.WaitFor(QueueType::Graphics, 0);

		const uint64_t first = gpu.Device.Submit(QueueType::Graphics, {});
		const uint64_t second = gpu.Device.Submit(QueueType::Graphics, {});
		CDL_EXPECT_EQ(first, 1u);
		CDL_EXPECT_EQ(second, 2u);

		gpu.Device.WaitFor(QueueType::Graphics, second);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Roles sharing one VkQueue share its timeline, so their values interleave; separate queues count independently.
CDL_TEST_CASE(RenderDevice, RolesOnOneQueueShareItsTimeline, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const bool aliased = gpu.Device.GetQueueFamily(QueueType::Transfer) == gpu.Device.GetQueueFamily(QueueType::Graphics);
		CDL_NOTE(aliased ? "transfer aliases the graphics queue" : "transfer has its own queue");

		const uint64_t graphics = gpu.Device.Submit(QueueType::Graphics, {});
		const uint64_t transfer = gpu.Device.Submit(QueueType::Transfer, {});
		const uint64_t expectedTransfer = aliased ? 2u : 1u;
		CDL_EXPECT_EQ(graphics, 1u);
		CDL_EXPECT_EQ(transfer, expectedTransfer);

		gpu.Device.WaitFor(QueueType::Graphics, graphics);
		gpu.Device.WaitFor(QueueType::Transfer, transfer);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The caller's binary semaphores ride along with the timeline, up to the limit. If Submit dropped one, the second
// batch would wait on a semaphore nothing signals: validation reports it at submit, and the GPU would never finish,
// so the messages are checked before waiting rather than hanging the run.
CDL_TEST_CASE(RenderDevice, PassesEverySignalThroughAlongsideTheTimeline, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		std::vector<vk::raii::Semaphore> semaphores;
		std::vector<vk::SemaphoreSubmitInfo> handoffs;
		for (size_t i = 0; i < SubmitDesc::kMaxSignals; ++i)
		{
			semaphores.emplace_back(gpu.Device.GetDevice(), vk::SemaphoreCreateInfo{});
			handoffs.push_back({ .semaphore = *semaphores.back(), .stageMask = vk::PipelineStageFlagBits2::eAllCommands });
		}

		gpu.Device.Submit(QueueType::Graphics, { .Signals = handoffs });
		const uint64_t last = gpu.Device.Submit(QueueType::Graphics, { .Waits = handoffs });

		ExpectNoVulkanDebugMessages(logs);
		CDL_CHECK_MSG(Context::Get().SoftFailures.empty(), "a signal went missing, so waiting on the GPU would never return");
		gpu.Device.WaitFor(QueueType::Graphics, last);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The batch waits on a timeline only the host signals, so the GPU cannot reach its value until the test allows it.
// "Not yet returned" is guaranteed whenever it is sampled, so the sleep only gives a broken WaitFor time to show.
CDL_TEST_CASE(RenderDevice, WaitForBlocksUntilTheGpuReachesTheValue, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		vk::SemaphoreTypeCreateInfo timelineType{ .semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = 0 };
		vk::raii::Semaphore gate(gpu.Device.GetDevice(), vk::SemaphoreCreateInfo{ .pNext = &timelineType });

		const vk::SemaphoreSubmitInfo gateWait{ .semaphore = *gate, .value = 1, .stageMask = vk::PipelineStageFlagBits2::eAllCommands };
		const uint64_t value = gpu.Device.Submit(QueueType::Graphics, { .Waits = { &gateWait, 1 } });

		std::atomic<bool> returned = false;
		std::jthread waiter([&] { gpu.Device.WaitFor(QueueType::Graphics, value); returned = true; });

		Platform::SleepCurrentThread(50);
		const bool returnedBeforeTheGateOpened = returned;

		gpu.Device.GetDevice().signalSemaphore({ .semaphore = *gate, .value = 1 });
		waiter.join();

		CDL_EXPECT_FALSE(returnedBeforeTheGateOpened);
		CDL_EXPECT(returned);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The completed value is a poll, so deletion queues can drain without waiting: it stays below a submit held back by a
// host-signaled gate, and reaches it once the GPU does.
CDL_TEST_CASE(RenderDevice, CompletedTimelineValueTracksTheGpu, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		CDL_EXPECT_EQ(gpu.Device.GetCompletedTimelineValue(QueueType::Graphics), 0u);

		vk::SemaphoreTypeCreateInfo timelineType{ .semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = 0 };
		vk::raii::Semaphore gate(gpu.Device.GetDevice(), vk::SemaphoreCreateInfo{ .pNext = &timelineType });
		const vk::SemaphoreSubmitInfo gateWait{ .semaphore = *gate, .value = 1, .stageMask = vk::PipelineStageFlagBits2::eAllCommands };
		const uint64_t value = gpu.Device.Submit(QueueType::Graphics, { .Waits = { &gateWait, 1 } });
		CDL_EXPECT_LT(gpu.Device.GetCompletedTimelineValue(QueueType::Graphics), value);

		gpu.Device.GetDevice().signalSemaphore({ .semaphore = *gate, .value = 1 });
		gpu.Device.WaitFor(QueueType::Graphics, value);
		CDL_EXPECT_EQ(gpu.Device.GetCompletedTimelineValue(QueueType::Graphics), value);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Many threads submitting at once, alternating roles. Per VkQueue the values must be unique and gap-free, and rise
// within each thread; validation's thread-safety checks fail the test if two submits ever touch one VkQueue together.
// Gpu rather than Stress, because a driver is the harder requirement.
CDL_TEST_CASE(RenderDevice, ConcurrentSubmitsGetUniqueRisingValues, Gpu)
{
	constexpr int kSubmitsPerThread = 64;

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const int threadCount = StressThreadCount();
		auto roleOf = [](int thread) { return thread % 2 == 0 ? QueueType::Graphics : QueueType::Transfer; };

		std::vector<std::vector<uint64_t>> values(threadCount);
		RunConcurrently(threadCount, [&](int thread)
		{
			values[thread].reserve(kSubmitsPerThread);
			for (int i = 0; i < kSubmitsPerThread; ++i)
				values[thread].push_back(gpu.Device.Submit(roleOf(thread), {}));
		});
		gpu.Device.WaitIdle();

		// Keyed by family rather than role, since aliased roles count on one timeline
		std::map<uint32_t, std::vector<uint64_t>> perQueue;
		for (int thread = 0; thread < threadCount; ++thread)
		{
			const std::vector<uint64_t>& mine = values[thread];
			CDL_EXPECT_MSG(std::ranges::adjacent_find(mine, std::greater_equal{}) == mine.end(), std::format("thread {}'s values did not rise", thread));

			std::vector<uint64_t>& queue = perQueue[gpu.Device.GetQueueFamily(roleOf(thread))];
			queue.insert(queue.end(), mine.begin(), mine.end());
		}

		for (auto& [family, queue] : perQueue)
		{
			std::ranges::sort(queue);
			std::vector<uint64_t> expected(queue.size());
			std::iota(expected.begin(), expected.end(), uint64_t{ 1 });
			CDL_EXPECT_MSG(queue == expected, std::format("family {}'s {} values are not 1..{}", family, queue.size(), queue.size()));
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// VK_EXT_memory_budget is optional, and VMA trusts the flag GPUAllocator derives from this, so it must be exact.
CDL_TEST_CASE(RenderDevice, ReportsMemoryBudgetExactlyWhenTheDeviceOffersIt, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const std::vector<vk::ExtensionProperties> extensions = gpu.Device.GetPhysicalDevice().enumerateDeviceExtensionProperties();
		const bool offered = std::ranges::any_of(extensions, [](const vk::ExtensionProperties& extension) {
			return std::string_view(extension.extensionName.data()) == vk::EXTMemoryBudgetExtensionName;
		});

		CDL_NOTE(std::format("VK_EXT_memory_budget {}", offered ? "offered" : "not offered"));
		CDL_EXPECT_EQ(gpu.Device.HasMemoryBudget(), offered);
	}
	ExpectNoVulkanDebugMessages(logs);
}
