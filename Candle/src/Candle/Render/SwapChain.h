#pragma once

#include "Image.h"

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>

#include <vector>

namespace Candle {

	class Window;
	class RenderInstance;
	class RenderDevice;

	struct ImageResult
	{
		vk::Result Result;
		uint32_t ImageIndex;
	};

	// Holds the vulkan swapchain, surface, images and image views; dynamic rendering needs no framebuffers
	// Also owns the binary semaphores handing images to and from presentation, which can't use timelines
	// Can be used on render thread only
	class SwapChain
	{
	public:
		SwapChain(RenderInstance& renderInstance, RenderDevice& renderDevice, Window& window);
		~SwapChain() = default;

		// Signals GetAcquireWait(frameIndex); the submit that last waited on it must have retired
		ImageResult AquireNextImage(uint32_t frameIndex, uint64_t timeout = UINT64_MAX);
		vk::Result Present(uint32_t imageIndex);	// Waits on GetPresentSignal(imageIndex)

		// The submit rendering to an acquired image must wait on the first and signal the second
		vk::SemaphoreSubmitInfo GetAcquireWait(uint32_t frameIndex) const;
		vk::SemaphoreSubmitInfo GetPresentSignal(uint32_t imageIndex) const;

		void Recreate(const glm::uvec2& extent);

		inline Image& GetImage(uint32_t imageIndex) { return m_SwapChainImages[imageIndex]; }
		inline const vk::SurfaceFormatKHR& GetSurfaceFormat() const { return m_SwapChainSurfaceFormat; }

	private:
		// Must be created on the main thread as it requires getting the window size from the windowing backend, which is main thread only.
		void CreateSwapChain(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities, const vk::Extent2D& extent);
		void CreateImages(const vk::Extent2D& extent);

	private:
		RenderDevice&			m_RenderDevice;
		vk::raii::SurfaceKHR	m_Surface{ nullptr };
		vk::raii::SwapchainKHR	m_SwapChain{ nullptr };
		vk::SurfaceFormatKHR	m_SwapChainSurfaceFormat;

		std::vector<Image> m_SwapChainImages;

		std::vector<vk::raii::Semaphore> m_AcquireSemaphores;	// Per frame in flight: needed before the image index is known
		std::vector<vk::raii::Semaphore> m_PresentSemaphores;	// Per image: presentation never reports when it is done with one
	};

}