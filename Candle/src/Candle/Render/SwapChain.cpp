#include "cdlpch.h"

#include "SwapChain.h"
#include "RenderInstance.h"
#include "RenderDevice.h"
#include "Image.h"

#include "Candle/Core/Window.h"
#include "Candle/Platform/Platform.h"

namespace Candle {

	namespace {

		constexpr uint32_t kDesiredMinImageCount = 3;

		// Choosing the best surface format is a matter of preference, but we will just pick the first one for now.
		// The 3 things to consider when choosing a surface format are:
		// 1. Surface format (color depth)
		// 2. Presentation mode(conditions for "swapping" images to the screen)
		// 3. Swap extent(resolution of images in swapchain)
		vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
			const auto formatIt = std::ranges::find_if(
				availableFormats,
				[](const auto& format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
			return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
		}

		// See for more options: https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/01_Swap_chain.html#_presentation_mode
		// eFifo because is always guaranteed to be available, but other modes may offer better performance.
		vk::PresentModeKHR ChooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes)
		{
			// Opt for eMailbox if available, otherwise fall back to eFifo. 
			// eMailbox is a low-latency mode that allows for triple buffering, while eFifo is a more traditional mode that uses double buffering.
			assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
			return std::ranges::any_of(availablePresentModes,
				[](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
				vk::PresentModeKHR::eMailbox :
				vk::PresentModeKHR::eFifo;
		}

		// The swap extent is the resolution of the images in the swap chain. The swap extent is determined by the surface capabilities and the window size.
		vk::Extent2D ChooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities, glm::uvec2 extent)
		{
			// currentExtent is only set to the special "undefined" value described above
			// when the window manager lets us choose the extent ourselves; any other value
			// means the surface already dictates a fixed extent that we must use as-is.
			if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
				return capabilities.currentExtent;

			return {
				std::clamp<uint32_t>(extent.x, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
				std::clamp<uint32_t>(extent.y, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
			};
		}

		// The minimum number of images in the swap chain is determined by the surface capabilities. 
		// We want to have at least 3 images in the swap chain to avoid stalling the GPU, 
		// but we also need to respect the maximum number of images allowed by the surface capabilities.
		uint32_t ChooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
		{

			auto minImageCount = std::max(kDesiredMinImageCount, surfaceCapabilities.minImageCount);
			if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
				minImageCount = surfaceCapabilities.maxImageCount;

			return minImageCount;
		}
	}


	SwapChain::SwapChain(RenderInstance& renderInstance, RenderDevice& renderDevice, Window& window)
		: m_RenderDevice(renderDevice), m_Surface(std::move(Platform::CreateVulkanSurface(window, renderInstance)))
	{
		// Only query the available formats once, as they are unlikely to change during the lifetime of the application.
		std::vector<vk::SurfaceFormatKHR> availableFormats = m_RenderDevice.GetPhysicalDevice().getSurfaceFormatsKHR(*m_Surface);
		m_SwapChainSurfaceFormat = ChooseSwapSurfaceFormat(availableFormats);

		Recreate(window.GetSize());
	}

	ImageResult SwapChain::AquireNextImage(vk::raii::Semaphore& signalSemaphore, uint64_t timeout /*= UINT64_MAX*/)
	{
		auto [result, index] = m_SwapChain.acquireNextImage(timeout, *signalSemaphore, nullptr);
		return { result, index };
	}

	void SwapChain::Recreate(const glm::uvec2& extent)
	{
		CreateSwapChain(extent);
		CreateImages();
	}

	void SwapChain::CreateSwapChain(const glm::uvec2& extent)
	{
		auto physicalDevice = m_RenderDevice.GetPhysicalDevice();

		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*m_Surface);
		std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*m_Surface);

		uint32_t minImageCount = ChooseSwapMinImageCount(surfaceCapabilities);
		vk::Extent2D swapChainExtent = ChooseSwapExtent(surfaceCapabilities, extent);

		// Create the swap chain using the chosen settings
		vk::SwapchainCreateInfoKHR swapChainCreateInfo{ .surface = *m_Surface,
											   .minImageCount = minImageCount,
											   .imageFormat = m_SwapChainSurfaceFormat.format,
											   .imageColorSpace = m_SwapChainSurfaceFormat.colorSpace,
											   .imageExtent = swapChainExtent,
											   .imageArrayLayers = 1,
											   .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
											   .imageSharingMode = vk::SharingMode::eExclusive,
											   .preTransform = surfaceCapabilities.currentTransform,
											   .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
											   .presentMode = ChooseSwapPresentMode(availablePresentModes),
											   .clipped = true };

		m_SwapChain = vk::raii::SwapchainKHR(m_RenderDevice.GetDevice(), swapChainCreateInfo);
	}

	void SwapChain::CreateImages()
	{
		std::vector<vk::Image> swapChainImages = m_SwapChain.getImages();
		vk::ImageViewCreateInfo imageViewCreateInfo{ .viewType = vk::ImageViewType::e2D,
													.format = m_SwapChainSurfaceFormat.format,
													.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1} };

		m_SwapChainImages.clear();
		m_SwapChainImages.reserve(swapChainImages.size());
		for (const auto& image : swapChainImages)
		{
			imageViewCreateInfo.image = image;

			vk::raii::ImageView imageView(m_RenderDevice.GetDevice(), imageViewCreateInfo);
			m_SwapChainImages.emplace_back(image, std::move(imageView));
		}
	}

}