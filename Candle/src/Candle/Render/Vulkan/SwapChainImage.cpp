#include "cdlpch.h"
#include "SwapChainImage.h"

namespace Candle {

	SwapChainImage::SwapChainImage(vk::Image image, vk::raii::ImageView imageView, const ImageDesc& desc)
		: m_Image(image), m_ImageView(std::move(imageView)), m_Desc(desc)
	{
	}

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
