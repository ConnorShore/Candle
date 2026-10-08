#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>

#include <vector>

namespace Candle {

	class Window;
	class RenderInstance;
	class RenderDevice;
	class Image;

	// Holds the vulkan swapchain, surface, images and image views; dynamic rendering needs no framebuffers
	// Can be used on render thread only
	class SwapChain
	{
	public:
		SwapChain(Window& window, RenderInstance& renderInstance, RenderDevice& renderDevice);
		~SwapChain();

	private:
		// Must be created on the main thread as it requires getting the window size from the windowing backend, which is main thread only.
		void CreateSwapChain(Window& window, RenderDevice& renderDevice);
		void CreateImages(RenderDevice& renderDevice);

	private:
		vk::raii::SurfaceKHR	m_Surface{ nullptr };
		vk::raii::SwapchainKHR	m_SwapChain{ nullptr };
		vk::SurfaceFormatKHR	m_SwapChainSurfaceFormat;
		glm::uvec2				m_Extent;

		std::vector<Image> m_SwapChainImages;
	};

}