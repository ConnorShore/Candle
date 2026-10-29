#include "cdlpch.h"

#include "SwapChain.h"
#include "RenderInstance.h"
#include "RenderDevice.h"
#include "RenderConstants.h"
#include "SwapChainImage.h"

#include "Candle/Core/Window.h"
#include "Candle/Platform/Platform.h"

namespace Candle {

	namespace {

		constexpr uint32_t kDesiredMinImageCount = 3;

		// An acquired image is first used as a color attachment; its first barrier starts here to chain with the wait
		constexpr vk::PipelineStageFlags2 kAcquireWaitStage = vk::PipelineStageFlagBits2::eColorAttachmentOutput;

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
		// eFifo is the only mode every device must support, and the only one that waits for vertical blank.
		// Without vsync prefer eMailbox, which doesn't tear, then eImmediate.
		vk::PresentModeKHR ChooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes, bool enableVSync)
		{
			if (enableVSync)
				return vk::PresentModeKHR::eFifo;

			if (std::ranges::contains(availablePresentModes, vk::PresentModeKHR::eMailbox))
				return vk::PresentModeKHR::eMailbox;
			if (std::ranges::contains(availablePresentModes, vk::PresentModeKHR::eImmediate))
				return vk::PresentModeKHR::eImmediate;

			return vk::PresentModeKHR::eFifo;
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


	SwapChain::SwapChain(RenderInstance& renderInstance, RenderDevice& renderDevice, Window& window, bool enableVSync)
		: m_RenderDevice(renderDevice), m_Surface(std::move(Platform::CreateVulkanSurface(window, renderInstance))), m_EnableVSync(enableVSync)
	{
		// Only query the available formats once, as they are unlikely to change during the lifetime of the application.
		std::vector<vk::SurfaceFormatKHR> availableFormats = m_RenderDevice.GetPhysicalDevice().getSurfaceFormatsKHR(*m_Surface);
		m_SwapChainSurfaceFormat = ChooseSwapSurfaceFormat(availableFormats);

		Recreate(window.GetSize());

		m_AcquireSemaphores.reserve(kMaxFramesInFlight);
		for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
		{
			m_AcquireSemaphores.emplace_back(m_RenderDevice.GetDevice(), vk::SemaphoreCreateInfo{});
			m_RenderDevice.SetDebugName(*m_AcquireSemaphores.back(), std::format("Acquire Semaphore [frame {}]", i));
		}
	}

	ImageResult SwapChain::AquireNextImage(uint32_t frameIndex, uint64_t timeout /*= UINT64_MAX*/)
	{
		CDL_PROFILE_WAIT("SwapChain::AquireNextImage");

		auto [result, index] = m_SwapChain.acquireNextImage(timeout, *m_AcquireSemaphores[frameIndex], nullptr);

		// Presentation leaves no barrier of ours to chain from; Undefined discards contents every frame overwrites
		if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR)
			m_SwapChainImages[index].SetState({ .Layout = vk::ImageLayout::eUndefined, .Access = vk::AccessFlagBits2::eNone, .Stage = kAcquireWaitStage });

		return { result, index };
	}

	vk::Result SwapChain::Present(uint32_t imageIndex)
	{
		CDL_PROFILE_WAIT("SwapChain::Present");

		vk::PresentInfoKHR presentInfo{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*m_PresentSemaphores[imageIndex],
			.swapchainCount = 1,
			.pSwapchains = &*m_SwapChain,
			.pImageIndices = &imageIndex };

		return m_RenderDevice.Present(presentInfo);
	}

	vk::SemaphoreSubmitInfo SwapChain::GetAcquireWait(uint32_t frameIndex) const
	{
		return { .semaphore = *m_AcquireSemaphores[frameIndex], .stageMask = kAcquireWaitStage };
	}

	vk::SemaphoreSubmitInfo SwapChain::GetPresentSignal(uint32_t imageIndex) const
	{
		// All commands, as a legacy vkQueueSubmit signal: presentation must see every write and the final layout transition
		return { .semaphore = *m_PresentSemaphores[imageIndex], .stageMask = vk::PipelineStageFlagBits2::eAllCommands };
	}

	void SwapChain::Recreate(const glm::uvec2& extent)
	{
		if (extent.x == 0 || extent.y == 0)
			return;

		auto physicalDevice = m_RenderDevice.GetPhysicalDevice();
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*m_Surface);
		vk::Extent2D swapChainExtent = ChooseSwapExtent(surfaceCapabilities, extent);

		CreateSwapChain(surfaceCapabilities, swapChainExtent);
		CreateImages(swapChainExtent);
		m_RequestedExtent = extent;
	}

	void SwapChain::CreateSwapChain(const vk::SurfaceCapabilitiesKHR& surfaceCapabilities, const vk::Extent2D& extent)
	{
		auto physicalDevice = m_RenderDevice.GetPhysicalDevice();

		std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*m_Surface);

		uint32_t minImageCount = ChooseSwapMinImageCount(surfaceCapabilities);

		// Create the swap chain using the chosen settings
		vk::SwapchainCreateInfoKHR swapChainCreateInfo{ .surface = *m_Surface,
											   .minImageCount = minImageCount,
											   .imageFormat = m_SwapChainSurfaceFormat.format,
											   .imageColorSpace = m_SwapChainSurfaceFormat.colorSpace,
											   .imageExtent = extent,
											   .imageArrayLayers = 1,
											   .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
											   .imageSharingMode = vk::SharingMode::eExclusive,
											   .preTransform = surfaceCapabilities.currentTransform,
											   .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
											   .presentMode = ChooseSwapPresentMode(availablePresentModes, m_EnableVSync),
											   .clipped = true,
											   .oldSwapchain = *m_SwapChain};

		m_SwapChain = vk::raii::SwapchainKHR(m_RenderDevice.GetDevice(), swapChainCreateInfo);
		m_RenderDevice.SetDebugName(*m_SwapChain, "Swapchain");
	}

	void SwapChain::CreateImages(const vk::Extent2D& extent)
	{
		std::vector<vk::Image> swapChainImages = m_SwapChain.getImages();
		vk::ImageViewCreateInfo imageViewCreateInfo{ .viewType = vk::ImageViewType::e2D,
													.format = m_SwapChainSurfaceFormat.format,
													.subresourceRange = SwapChainImage::kSubresourceRange };

		m_SwapChainImages.clear();
		m_PresentSemaphores.clear();
		m_SwapChainImages.reserve(swapChainImages.size());
		m_PresentSemaphores.reserve(swapChainImages.size());
		for (uint32_t i = 0; i < swapChainImages.size(); ++i)
		{
			vk::Image image = swapChainImages[i];
			imageViewCreateInfo.image = image;

			vk::raii::ImageView imageView(m_RenderDevice.GetDevice(), imageViewCreateInfo);
			m_RenderDevice.SetDebugName(image, std::format("Swapchain Image {}", i));
			m_RenderDevice.SetDebugName(*imageView, std::format("Swapchain Image {} View", i));

			ImageDesc desc{ .Extent = extent, .Format = m_SwapChainSurfaceFormat.format };
			m_SwapChainImages.emplace_back(image, std::move(imageView), desc);
			m_PresentSemaphores.emplace_back(m_RenderDevice.GetDevice(), vk::SemaphoreCreateInfo{});
			m_RenderDevice.SetDebugName(*m_PresentSemaphores.back(), std::format("Present Semaphore [image {}]", i));
		}
	}

}