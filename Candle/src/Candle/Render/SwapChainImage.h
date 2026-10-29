#pragma once

#include "Image.h"

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	// One swapchain image and the state it was last left in. The swapchain owns the VkImage; this owns only the view.
	// It tracks state because its last use, presentation, happens outside our command stream, so no call site knows it.
	// Threading: render thread only, like the SwapChain holding it.
	class SwapChainImage
	{
	public:
		// One colour mip in one layer, as SwapChain creates them; the view and every barrier cover exactly this
		static constexpr vk::ImageSubresourceRange kSubresourceRange{ .aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1 };

		SwapChainImage(vk::Image image, vk::raii::ImageView imageView, const ImageDesc& desc);
		~SwapChainImage() = default;

		// Delete copy constructor and assignment operator
		SwapChainImage(const SwapChainImage&) = delete;
		SwapChainImage& operator=(const SwapChainImage&) = delete;

		// Allow move constructor and assignment operator
		SwapChainImage(SwapChainImage&&) = default;
		SwapChainImage& operator=(SwapChainImage&&) = default;

		void Transition(const ImageState& to, vk::raii::CommandBuffer& commandBuffer);	// Records a barrier from the tracked state

		// Overwrites the tracked state without a barrier; for an image last used outside our command stream, e.g. by presentation
		void SetState(const ImageState& state);

		inline vk::raii::ImageView& GetImageView() { return m_ImageView; }
		inline ImageDesc& GetDesc() { return m_Desc; }
		inline const ImageDesc& GetDesc() const { return m_Desc; }

	private:
		vk::Image m_Image;
		vk::raii::ImageView m_ImageView{ nullptr };

		ImageDesc m_Desc;
		ImageState m_State;	// SwapChain::AquireNextImage overwrites it on every acquire
	};

}
