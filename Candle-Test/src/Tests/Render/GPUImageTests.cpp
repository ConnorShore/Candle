// GPUImage over a real VMA allocator: one allocation per image, owned by exactly one GPUImage however it is moved.
// Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/GPUAllocator.h>
#include <Candle/Render/GPUImage.h>

#include <type_traits>
#include <utility>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;
using Candle::Test::Type::Unit;

// A copy would be a second owner destroying the same VkImage and VmaAllocation.
CDL_STATIC_ASSERT(!std::is_copy_constructible_v<GPUImage>);
CDL_STATIC_ASSERT(!std::is_copy_assignable_v<GPUImage>);
CDL_STATIC_ASSERT(std::is_nothrow_move_constructible_v<GPUImage>);
CDL_STATIC_ASSERT(std::is_nothrow_move_assignable_v<GPUImage>);

namespace {

	GPUImage CreateImage(GPUAllocator& allocator)
	{
		return allocator.CreateImage({ .m_Extent = { 4, 2 }, .m_Format = vk::Format::eR8G8B8A8Unorm });
	}

	// Every implementation must support D32 or X8_D24 as a depth attachment; desktop GPUs all have D32, as Renderer assumes.
	const GPUImageSpec kDepthSpec{
		.m_Extent = { 4, 2 },
		.m_Format = vk::Format::eD32Sfloat,
		.m_Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
		.m_Aspect = vk::ImageAspectFlagBits::eDepth };

	VkDeviceSize AllocationSize(GPUAllocator& allocator, const GPUImage& image)
	{
		VmaAllocationInfo info{};
		vmaGetAllocationInfo(allocator.GetAllocator(), image.GetAllocation(), &info);
		return info.size;
	}

}

// TexturePool names neither field, so a texture is only sampleable and uploadable while these stay the defaults.
CDL_TEST_CASE(GPUImage, DefaultSpecIsAnUploadableSampledColorImage, Unit)
{
	const GPUImageSpec spec;

	CDL_EXPECT(spec.m_Usage == (vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst));
	CDL_EXPECT(spec.m_Aspect == vk::ImageAspectFlags{ vk::ImageAspectFlagBits::eColor });
}

// A depth view with the colour aspect fails validation, so this fails if the spec's usage or aspect stops reaching Vulkan.
CDL_TEST_CASE(GPUImage, CreatesADepthImageFromItsSpec, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		{
			GPUImage depth = gpu.Allocator.CreateImage(kDepthSpec);

			CDL_EXPECT(depth.GetImage());
			CDL_EXPECT(depth.GetImageView());
			CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
		}
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Renderer resizes its depth target this way on every swapchain rebuild, so the old image must go and the new one take
// the new spec. Grown far past the original, so an allocation too small for it means the old extent was kept.
CDL_TEST_CASE(GPUImage, RecreateFreesTheOldImageAndTakesTheNewSpec, Gpu)
{
	const glm::uvec2 kGrownExtent{ 256, 256 };

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		{
			GPUImage image = gpu.Allocator.CreateImage(kDepthSpec);
			GPUImageSpec grown = kDepthSpec;
			grown.m_Extent = kGrownExtent;

			image.Recreate(grown, gpu.Allocator);

			CDL_EXPECT(image.GetImage());
			CDL_EXPECT(image.GetImageView());
			CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
			CDL_EXPECT_GE(AllocationSize(gpu.Allocator, image), VkDeviceSize{ kGrownExtent.x } * kGrownExtent.y * sizeof(float));
		}
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Neither image has a VmaAllocator until Recreate hands it one; VMA asserts on a null one, so a regression aborts the run.
CDL_TEST_CASE(GPUImage, RecreateWorksOnAnImageThatOwnsNothing, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		{
			GPUImage defaulted;
			GPUImage movedFrom = CreateImage(gpu.Allocator);
			const GPUImage keeper(std::move(movedFrom));

			defaulted.Recreate(kDepthSpec, gpu.Allocator);
			movedFrom.Recreate(kDepthSpec, gpu.Allocator);

			CDL_EXPECT(defaulted.GetImage());
			CDL_EXPECT(movedFrom.GetImage());
			CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 3u);
		}
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(GPUImage, CreatesAnImageWithAViewInOneAllocation, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		{
			GPUImage image = CreateImage(gpu.Allocator);

			CDL_EXPECT(image.GetImage());
			CDL_EXPECT(image.GetImageView());
			CDL_EXPECT(image.GetAllocation() != nullptr);
			CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
		}
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The state a moved-from image is left in, as TexturePool leaves one each time it moves a new texture into its slot.
CDL_TEST_CASE(GPUImage, DefaultConstructedOwnsNothing, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		{
			const GPUImage empty;

			CDL_EXPECT_FALSE(empty.GetImage());
			CDL_EXPECT_FALSE(empty.GetImageView());
			CDL_EXPECT(empty.GetAllocation() == nullptr);
		}
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The source is left owning nothing, so its destructor frees nothing and the image survives it.
CDL_TEST_CASE(GPUImage, MoveConstructionTransfersOwnership, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUImage source = CreateImage(gpu.Allocator);
		const vk::Image image = source.GetImage();
		const vk::ImageView view = source.GetImageView();
		{
			GPUImage moved(std::move(source));

			CDL_EXPECT(moved.GetImage() == image);
			CDL_EXPECT(moved.GetImageView() == view);
			CDL_EXPECT_FALSE(source.GetImage());
			CDL_EXPECT_FALSE(source.GetImageView());
			CDL_EXPECT(source.GetAllocation() == nullptr);
			CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
		}
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 0u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The image being overwritten must be freed, or every texture reloaded into a slot would leak its predecessor.
CDL_TEST_CASE(GPUImage, MoveAssignmentFreesTheImageItReplaces, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestAllocator gpu;
		GPUImage target = CreateImage(gpu.Allocator);
		GPUImage source = CreateImage(gpu.Allocator);
		const vk::Image image = source.GetImage();
		CDL_CHECK_EQ(LiveAllocations(gpu.Allocator), 2u);

		target = std::move(source);

		CDL_EXPECT(target.GetImage() == image);
		CDL_EXPECT_FALSE(source.GetImage());
		CDL_EXPECT_EQ(LiveAllocations(gpu.Allocator), 1u);
	}
	ExpectNoVulkanDebugMessages(logs);
}
