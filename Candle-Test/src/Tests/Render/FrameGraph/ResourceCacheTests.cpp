// The cache keeps handing a transient key the same image, and frees one no frame has asked for once the GPU is past it.
// It only compares timeline values, never waits on them, so these pass made-up ones and submit nothing.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Render/FrameGraph/ResourceCache.h>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	constexpr uint64_t kWindow = ResourceCache::kMaxUnusedFramesBeforeEviction;

	constexpr FGTextureKey kColorKey{
		.Format = vk::Format::eR8G8B8A8Unorm,
		.Extent = { 8, 4 },
		.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled };

	constexpr FGTextureKey kOtherKey{
		.Format = vk::Format::eR8G8B8A8Unorm,
		.Extent = { 16, 16 },
		.Usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled };

	struct TestCache : TestAllocator
	{
		BindlessTable Bindless{ Device };
		ResourceCache Cache{ Allocator, Bindless };
	};

}

CDL_TEST_CASE(ResourceCache, KeepsAnEntryUnusedForNoMoreThanTheWindow, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestCache test;
		const CacheImportResult first = test.Cache.CreateOrGetTexture(kColorKey, 0);
		const uint32_t allocations = LiveAllocations(test.Allocator);

		// The GPU is always caught up, so only the window keeps it alive
		for (uint64_t frame = 1; frame <= kWindow; ++frame)
			test.Cache.EvictUnusedEntries(frame, frame, frame);

		const CacheImportResult later = test.Cache.CreateOrGetTexture(kColorKey, kWindow + 1);
		CDL_EXPECT(later.Image == first.Image);
		CDL_EXPECT_EQ(later.CacheIndex, first.CacheIndex);
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), allocations);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Frames that used it may still be running, so it waits for the submit that was last when it started retiring
CDL_TEST_CASE(ResourceCache, FreesAnUnusedEntryOnlyOnceTheGPUReachesItsRetireValue, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestCache test;
		const uint32_t before = LiveAllocations(test.Allocator);
		test.Cache.CreateOrGetTexture(kColorKey, 0);
		CDL_CHECK_EQ(LiveAllocations(test.Allocator), before + 1);

		test.Cache.EvictUnusedEntries(kWindow + 1, 10, 9);
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), before + 1);

		// A later submit doesn't push it back: the retire value was fixed when it started retiring
		test.Cache.EvictUnusedEntries(kWindow + 2, 20, 10);
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), before);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Its image and descriptor are untouched until it's freed, so asking for the key again before then costs nothing
CDL_TEST_CASE(ResourceCache, BringsBackARetiringEntryAskedForAgain, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestCache test;
		const CacheImportResult first = test.Cache.CreateOrGetTexture(kColorKey, 0);
		const uint32_t allocations = LiveAllocations(test.Allocator);

		test.Cache.EvictUnusedEntries(kWindow + 1, 10, 9);
		const CacheImportResult revived = test.Cache.CreateOrGetTexture(kColorKey, kWindow + 2);
		CDL_EXPECT(revived.Image == first.Image);
		CDL_EXPECT_EQ(revived.CacheIndex, first.CacheIndex);
		CDL_EXPECT_EQ(revived.BindlessIndex, first.BindlessIndex);

		// The GPU is past its old retire value, but it's live again
		test.Cache.EvictUnusedEntries(kWindow + 2, 20, 10);
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), allocations);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A freed entry keeps its key, so matching on the key alone would hand out an image that no longer exists
CDL_TEST_CASE(ResourceCache, NeverHandsOutAFreedEntry, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestCache test;
		const uint32_t before = LiveAllocations(test.Allocator);
		test.Cache.CreateOrGetTexture(kColorKey, 0);
		test.Cache.EvictUnusedEntries(kWindow + 1, 1, 1);
		CDL_CHECK_EQ(LiveAllocations(test.Allocator), before);

		const CacheImportResult again = test.Cache.CreateOrGetTexture(kColorKey, kWindow + 2);
		CDL_EXPECT(again.Image != vk::Image{});
		CDL_EXPECT(again.ImageView != vk::ImageView{});
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), before + 1);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Slots index the frame graph's bindless range, so one that's never reused is lost for good
CDL_TEST_CASE(ResourceCache, GivesAFreedSlotToTheNextNewEntry, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestCache test;
		const CacheImportResult freed = test.Cache.CreateOrGetTexture(kColorKey, 0);
		test.Cache.EvictUnusedEntries(kWindow + 1, 1, 1);

		const CacheImportResult next = test.Cache.CreateOrGetTexture(kOtherKey, kWindow + 2);
		CDL_EXPECT_EQ(next.CacheIndex, freed.CacheIndex);
		CDL_EXPECT_EQ(next.BindlessIndex, freed.BindlessIndex);
	}
	ExpectNoVulkanDebugMessages(logs);
}
