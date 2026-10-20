// GPUUploader on a real device: staged copies land in every destination, read back through host-visible buffers.
// Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/GPUBuffer.h>
#include <Candle/Render/GPUUploader.h>

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

		CDL_EXPECT(ReadBack(gpu.Allocator, firstDestination) == first);
		CDL_EXPECT(ReadBack(gpu.Allocator, secondDestination) == second);
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 2u);	// The staging buffer is gone once the upload returns
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
