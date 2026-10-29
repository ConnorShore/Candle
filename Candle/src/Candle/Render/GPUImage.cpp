#include "cdlpch.h"
#include "GPUImage.h"
#include "GPUAllocator.h"
#include "RenderDevice.h"

namespace Candle {

	GPUImage::GPUImage(const GPUImageSpec& spec, GPUAllocator& allocator)
	{
		Create(spec, allocator);
	}

	GPUImage::~GPUImage()
	{
		Destroy();
	}

	void GPUImage::Recreate(const GPUImageSpec& spec, GPUAllocator& allocator)
	{
		Destroy();
		Create(spec, allocator);
	}

	void GPUImage::Create(const GPUImageSpec& spec, GPUAllocator& allocator)
	{
		// From the argument, not the constructor: a default-constructed or moved-from image has none until Recreate gives it one
		m_Allocator = allocator.GetAllocator();

		vk::ImageCreateInfo imageInfo{
			.imageType = vk::ImageType::e2D,
			.format = spec.m_Format,
			.extent = vk::Extent3D{ spec.m_Extent.x, spec.m_Extent.y, 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = spec.m_Usage,
			.sharingMode = vk::SharingMode::eExclusive,
			.initialLayout = vk::ImageLayout::eUndefined
		};

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = spec.m_Flags;

		VkResult result = vmaCreateImage(m_Allocator, reinterpret_cast<VkImageCreateInfo*>(&imageInfo), &allocInfo, reinterpret_cast<VkImage*>(&m_Image), &m_Allocation, nullptr);
		if (result != VK_SUCCESS)
		{
			CDL_CORE_ERROR(LogChannel::Memory, "Failed to create GPU image!");
			throw std::runtime_error("Failed to create GPU image");
		}

		m_ImageView = vk::raii::ImageView(allocator.GetDevice()->GetDevice(), vk::ImageViewCreateInfo{
			.image = m_Image,
			.viewType = vk::ImageViewType::e2D,
			.format = spec.m_Format,
			.subresourceRange = vk::ImageSubresourceRange{
				.aspectMask = spec.m_Aspect,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		});
	}

	void GPUImage::Destroy()
	{
		m_ImageView.clear();	// Clear the image view first, as it depends on the image

		if (m_Allocation)
			vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
	}

}