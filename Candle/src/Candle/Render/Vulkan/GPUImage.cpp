#include "cdlpch.h"
#include "GPUImage.h"
#include "Candle/Render/Vulkan/GPUAllocator.h"
#include "Candle/Render/Vulkan/RenderDevice.h"

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
			.format = spec.Format,
			.extent = vk::Extent3D{ spec.Extent.x, spec.Extent.y, 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = spec.Usage,
			.sharingMode = vk::SharingMode::eExclusive,
			.initialLayout = vk::ImageLayout::eUndefined
		};

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = spec.Flags;

		VkResult result = vmaCreateImage(m_Allocator, reinterpret_cast<VkImageCreateInfo*>(&imageInfo), &allocInfo, reinterpret_cast<VkImage*>(&m_Image), &m_Allocation, nullptr);
		if (result != VK_SUCCESS)
			CDL_CORE_PANIC(LogChannel::Memory, "Failed to create GPU image: {}", vk::to_string(static_cast<vk::Result>(result)));

		m_ImageView = vk::raii::ImageView(allocator.GetDevice()->GetDevice(), vk::ImageViewCreateInfo{
			.image = m_Image,
			.viewType = vk::ImageViewType::e2D,
			.format = spec.Format,
			.subresourceRange = vk::ImageSubresourceRange{
				.aspectMask = spec.Aspect,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		});

		if (!spec.DebugName.empty())
		{
			allocator.GetDevice()->SetDebugName(m_Image, spec.DebugName);
			allocator.GetDevice()->SetDebugName(*m_ImageView, spec.DebugName + " View");
			vmaSetAllocationName(m_Allocator, m_Allocation, spec.DebugName.c_str());
		}
	}

	void GPUImage::Destroy()
	{
		m_ImageView.clear();	// Clear the image view first, as it depends on the image

		if (m_Allocation)
			vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
	}

}