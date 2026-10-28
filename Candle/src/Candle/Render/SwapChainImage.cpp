#include "cdlpch.h"
#include "SwapChainImage.h"

namespace Candle {

	SwapChainImage::SwapChainImage(vk::Image image, vk::raii::ImageView imageView, const ImageDesc& desc)
		: m_Image(image), m_ImageView(std::move(imageView)), m_Desc(desc)
	{
	}

	// TODO: May change this so its more granular
	// (instead of saving state as per: https://docs.vulkan.org/tutorial/latest/Synchronization/Pipeline_Barriers_Transitions/02_image_barrier.html#_the_power_of_layout_discard)
	// but it may still be good to save state, will need to research further
	void SwapChainImage::Transition(const ImageState& to, vk::raii::CommandBuffer& commandBuffer)
	{
		const vk::ImageMemoryBarrier2 barrier = MakeImageBarrier(m_Image, m_State, to, kSubresourceRange);
		commandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier });

		m_State = to;
	}

	void SwapChainImage::SetState(const ImageState& state)
	{
		m_State = state;
	}

}
