#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	class Image
	{
	public:
		Image(vk::Image image, vk::raii::ImageView imageView);
		~Image() = default;

		// Delete copy constructor and assignment operator
		Image(const Image&) = delete;
		Image& operator=(const Image&) = delete;

		// Allow move constructor and assignment operator
		Image(Image&&) = default;
		Image& operator=(Image&&) = default;

		inline vk::raii::ImageView& GetImageView() { return m_ImageView; }

	private:
		vk::Image m_Image;
		vk::raii::ImageView m_ImageView{ nullptr };
	};

}