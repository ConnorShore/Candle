#pragma once

#include "Image.h"

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>

#include <vector>

namespace Candle {

	class Window;
	class RenderInstance;
	class RenderDevice;

	// Holds the vulkan swapchain, surface, images and image views; dynamic rendering needs no framebuffers
	// Can be used on render thread only
	class SwapChain
	{
	public:
		SwapChain(RenderInstance& renderInstance, RenderDevice& renderDevice, Window& window);
		~SwapChain() = default;

		void Recreate(const glm::uvec2& extent);

		inline const vk::SurfaceFormatKHR& GetSurfaceFormat() const { return m_SwapChainSurfaceFormat; }

	private:
		// Must be created on the main thread as it requires getting the window size from the windowing backend, which is main thread only.
		void CreateSwapChain(const glm::uvec2& extent);
		void CreateImages();

	private:
		RenderDevice&			m_RenderDevice;
		vk::raii::SurfaceKHR	m_Surface{ nullptr };
		vk::raii::SwapchainKHR	m_SwapChain{ nullptr };
		vk::SurfaceFormatKHR	m_SwapChainSurfaceFormat;

		std::vector<Image> m_SwapChainImages;
	};

}