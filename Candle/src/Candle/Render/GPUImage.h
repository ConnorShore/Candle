#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>

namespace Candle {

	class GPUAllocator;

	struct GPUImageSpec
	{
		glm::uvec2 m_Extent{ 1, 1 };
		vk::Format m_Format{ vk::Format::eR8G8B8A8Unorm };
		VmaAllocationCreateFlags m_Flags{ VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT };
	};

	class GPUImage
	{
	public:
		GPUImage() = default;
		GPUImage(const GPUImageSpec& spec, GPUAllocator& allocator);
		~GPUImage();

		// Delete copy constructor and assignment operator
		GPUImage(const GPUImage&) = delete;
		GPUImage& operator=(const GPUImage&) = delete;

		// Move constructor and assignment operator
		GPUImage(GPUImage&& other) noexcept
			: m_Allocator(other.m_Allocator), m_Image(other.m_Image), m_ImageView(std::move(other.m_ImageView)), m_Allocation(other.m_Allocation), m_Extent(other.m_Extent)
		{
			other.m_Image = nullptr;
			other.m_Allocator = nullptr;
			other.m_Allocation = nullptr;
			other.m_Extent = { 1, 1 };
		}
		inline GPUImage& operator=(GPUImage&& other) noexcept
		{
			if (this != &other)
			{
				Destroy();	// Otherwise the texture being overwritten leaks
				m_Allocator = other.m_Allocator;
				m_Image = other.m_Image;
				m_ImageView = std::move(other.m_ImageView);
				m_Allocator = other.m_Allocator;
				m_Allocation = other.m_Allocation;
				other.m_Allocator = nullptr;
				other.m_Allocation = nullptr;
				other.m_Image = nullptr;
			}
			return *this;
		}

		inline vk::Image GetImage() const { return m_Image; }
		inline vk::raii::ImageView& GetImageView() { return m_ImageView; }
		inline VmaAllocation GetAllocation() const { return m_Allocation; }

	private:
		void Destroy();

	private:
		VmaAllocator m_Allocator{ nullptr };
		vk::Image m_Image{ nullptr };
		VmaAllocation m_Allocation{ nullptr };
		vk::raii::ImageView m_ImageView{ nullptr };
		glm::uvec2 m_Extent{ 1, 1 };
	};

}