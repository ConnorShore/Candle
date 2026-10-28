// GPUUploader on a real device: staged copies land in every destination, buffers and images alike, read back through
// host-visible buffers, and staging lives until the copy is done. Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/GPUBuffer.h>
#include <Candle/Render/GPUUploader.h>
#include <Candle/Render/Image.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	// The destinations are read on the host, so the upload's barrier hands them to host reads.
	void Upload(GPUUploader& uploader, std::span<const BufferUpload> uploads)
	{
		uploader.UploadBuffers(uploads, vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead);
	}

	// Uploads don't block; the GPU reaches a later submit only once every earlier one on the queue is done.
	void WaitForGPU(TestAllocator& gpu)
	{
		gpu.Device.WaitFor(QueueType::Graphics, gpu.Device.Submit(QueueType::Graphics, {}));
	}

	// The test copies the image out next, so the upload's last barrier hands it to transfer reads in TransferSrc.
	void UploadImage(GPUUploader& uploader, std::span<const std::byte> data, vk::Image destination, glm::uvec2 extent)
	{
		uploader.UploadImage({ .Data = data, .Destination = destination, .DestinationLayout = kTransferSrcState.Layout, .Extent = extent },
			kTransferSrcState.Stage, kTransferSrcState.Access);
	}

	// Copied into by the uploader and out of by the test; outside VMA, so LiveAllocations counts only staging.
	OwnedImage CreateUploadTarget(RenderDevice& device, vk::Extent2D extent)
	{
		return CreateOwnedImage(device, vk::Format::eR8G8B8A8Unorm, extent, 1,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc);
	}

}

// Two regions of different sizes share one staging buffer, so the second only arrives intact if its staging offset is right.
CDL_TEST_CASE(GPUUploader, CopiesEveryRegionToItsDestination, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUUploader uploader(gpu.Device, gpu.Allocator);

		const std::vector<std::byte> first = BytePattern(64, 1);
		const std::vector<std::byte> second = BytePattern(160, 100);
		GPUBuffer firstDestination = CreateHostBuffer(gpu.Allocator, first.size());
		GPUBuffer secondDestination = CreateHostBuffer(gpu.Allocator, second.size());

		const std::array<BufferUpload, 2> uploads = {
			BufferUpload{ .Data = first, .Destination = firstDestination.GetBuffer() },
			BufferUpload{ .Data = second, .Destination = secondDestination.GetBuffer() } };
		Upload(uploader, uploads);
		WaitForGPU(gpu);

		CDL_EXPECT(ReadBack(gpu.Allocator, firstDestination) == first);
		CDL_EXPECT(ReadBack(gpu.Allocator, secondDestination) == second);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The copy reads the staging buffer after UploadBuffers returns, so only a drain once the GPU is done frees it.
CDL_TEST_CASE(GPUUploader, FreesStagingOnlyWhenDrainedAfterTheCopy, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUUploader uploader(gpu.Device, gpu.Allocator);
		const std::vector<std::byte> data = BytePattern(32, 7);
		GPUBuffer destination = CreateHostBuffer(gpu.Allocator, data.size());

		const std::array<BufferUpload, 1> uploads = { BufferUpload{ .Data = data, .Destination = destination.GetBuffer() } };
		Upload(uploader, uploads);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 2u);	// The destination and the staging buffer

		WaitForGPU(gpu);
		uploader.FreeCompletedStagingBuffers();
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The second upload resets the command pool the first was recorded from, so it must wait for the first to finish.
CDL_TEST_CASE(GPUUploader, BackToBackUploadsBothLand, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUUploader uploader(gpu.Device, gpu.Allocator);
		const std::vector<std::byte> first = BytePattern(64, 3);
		const std::vector<std::byte> second = BytePattern(64, 200);
		GPUBuffer firstDestination = CreateHostBuffer(gpu.Allocator, first.size());
		GPUBuffer secondDestination = CreateHostBuffer(gpu.Allocator, second.size());

		Upload(uploader, std::array{ BufferUpload{ .Data = first, .Destination = firstDestination.GetBuffer() } });
		Upload(uploader, std::array{ BufferUpload{ .Data = second, .Destination = secondDestination.GetBuffer() } });
		WaitForGPU(gpu);

		CDL_EXPECT(ReadBack(gpu.Allocator, firstDestination) == first);
		CDL_EXPECT(ReadBack(gpu.Allocator, secondDestination) == second);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(GPUUploader, SkipsEmptyRegions, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUUploader uploader(gpu.Device, gpu.Allocator);

		const std::vector<std::byte> untouched(32, std::byte{ 0 });
		const std::vector<std::byte> data = BytePattern(48, 9);
		GPUBuffer skipped = CreateHostBuffer(gpu.Allocator, untouched.size());
		GPUBuffer written = CreateHostBuffer(gpu.Allocator, data.size());
		skipped.Write(untouched);

		const std::array<BufferUpload, 2> uploads = {
			BufferUpload{ .Data = {}, .Destination = skipped.GetBuffer() },
			BufferUpload{ .Data = data, .Destination = written.GetBuffer() } };
		Upload(uploader, uploads);
		WaitForGPU(gpu);

		CDL_EXPECT(ReadBack(gpu.Allocator, skipped) == untouched);
		CDL_EXPECT(ReadBack(gpu.Allocator, written) == data);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Nothing to copy means nothing submitted: an empty submit either side sees consecutive timeline values.
CDL_TEST_CASE(GPUUploader, SubmitsNothingWhenEveryRegionIsEmpty, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUUploader uploader(gpu.Device, gpu.Allocator);
		GPUBuffer destination = CreateHostBuffer(gpu.Allocator, 16);

		const uint64_t before = gpu.Device.Submit(QueueType::Graphics, {});
		const std::array<BufferUpload, 1> uploads = { BufferUpload{ .Data = {}, .Destination = destination.GetBuffer() } };
		Upload(uploader, uploads);
		const uint64_t after = gpu.Device.Submit(QueueType::Graphics, {});

		CDL_EXPECT_EQ(after, before + 1);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
		gpu.Device.WaitFor(QueueType::Graphics, after);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// With uploads no longer blocking, the destructor is the only wait left: without it, the copy still in flight would
// have its staging buffer and command pool destroyed under it, which validation reports.
CDL_TEST_CASE(GPUUploader, DestroyingItWithAnUploadInFlightIsClean, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		const std::vector<std::byte> data = BytePattern(4096, 11);
		GPUBuffer destination = CreateHostBuffer(gpu.Allocator, data.size());
		{
			GPUUploader uploader(gpu.Device, gpu.Allocator);
			Upload(uploader, std::array{ BufferUpload{ .Data = data, .Destination = destination.GetBuffer() } });
		}

		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);	// The staging went with the uploader
		CDL_EXPECT(ReadBack(gpu.Allocator, destination) == data);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Wider than tall, so swapped extents or a wrong row pitch scramble the readback instead of matching by symmetry.
CDL_TEST_CASE(GPUUploader, UploadImageCopiesEveryTexel, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUUploader uploader(gpu.Device, gpu.Allocator);
		constexpr vk::Extent2D kExtent{ 3, 2 };
		const std::vector<std::byte> texels = BytePattern(kExtent.width * kExtent.height * sizeof(Texel), 21);
		OwnedImage image = CreateUploadTarget(gpu.Device, kExtent);

		UploadImage(uploader, texels, *image.Handle, { kExtent.width, kExtent.height });

		CDL_EXPECT(ReadBackImage(gpu, *image.Handle, kExtent, kTransferSrcState) == texels);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// As with buffers: the copy reads the staging buffer after UploadImage returns, so only a drain once it is done frees it.
CDL_TEST_CASE(GPUUploader, UploadImageFreesStagingOnlyWhenDrainedAfterTheCopy, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUUploader uploader(gpu.Device, gpu.Allocator);
		OwnedImage image = CreateUploadTarget(gpu.Device, { 2, 2 });

		UploadImage(uploader, BytePattern(2 * 2 * sizeof(Texel), 3), *image.Handle, { 2, 2 });
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);

		WaitForGPU(gpu);
		uploader.FreeCompletedStagingBuffers();
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(GPUUploader, UploadImageSubmitsNothingForEmptyData, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUUploader uploader(gpu.Device, gpu.Allocator);
		OwnedImage image = CreateUploadTarget(gpu.Device, { 1, 1 });

		const uint64_t before = gpu.Device.Submit(QueueType::Graphics, {});
		UploadImage(uploader, {}, *image.Handle, { 1, 1 });
		const uint64_t after = gpu.Device.Submit(QueueType::Graphics, {});

		CDL_EXPECT_EQ(after, before + 1);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
		gpu.Device.WaitFor(QueueType::Graphics, after);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// UploadImage has its own copy of the wait-then-reset, so the pool reset must hold whichever kind of upload came first.
CDL_TEST_CASE(GPUUploader, ImageAndBufferUploadsBackToBackBothLand, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUUploader uploader(gpu.Device, gpu.Allocator);
		constexpr vk::Extent2D kExtent{ 2, 2 };
		const std::vector<std::byte> texels = BytePattern(kExtent.width * kExtent.height * sizeof(Texel), 40);
		const std::vector<std::byte> before = BytePattern(64, 90);
		const std::vector<std::byte> after = BytePattern(64, 160);
		OwnedImage image = CreateUploadTarget(gpu.Device, kExtent);
		GPUBuffer beforeDestination = CreateHostBuffer(gpu.Allocator, before.size());
		GPUBuffer afterDestination = CreateHostBuffer(gpu.Allocator, after.size());

		Upload(uploader, std::array{ BufferUpload{ .Data = before, .Destination = beforeDestination.GetBuffer() } });
		UploadImage(uploader, texels, *image.Handle, { kExtent.width, kExtent.height });
		Upload(uploader, std::array{ BufferUpload{ .Data = after, .Destination = afterDestination.GetBuffer() } });
		WaitForGPU(gpu);

		CDL_EXPECT(ReadBack(gpu.Allocator, beforeDestination) == before);
		CDL_EXPECT(ReadBackImage(gpu, *image.Handle, kExtent, kTransferSrcState) == texels);
		CDL_EXPECT(ReadBack(gpu.Allocator, afterDestination) == after);
	}
	ExpectNoVulkanDebugMessages(logs);
}
