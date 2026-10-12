#include "cdlpch.h"
#include "Image.h"

namespace Candle {

	Image::Image(vk::Image image, vk::raii::ImageView imageView, const ImageDesc& desc)
		: m_Image(std::move(image)), m_ImageView(std::move(imageView)), m_Desc(desc)
	{
	}

	void Image::Transition(const ImageTransitionParams& params, vk::raii::CommandBuffer& commandBuffer)
	{
		vk::ImageMemoryBarrier2 barrier = {
			.srcStageMask = m_CurrentStageMask,
			.srcAccessMask = m_CurrentAccessMask,
			.dstStageMask = params.StageMask,
			.dstAccessMask = params.AccessMask,
			.oldLayout = m_CurrentLayout,
			.newLayout = params.Layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = m_Image,
			.subresourceRange = {
				   .aspectMask = params.AspectMask,
				   .baseMipLevel = 0,
				   .levelCount = 1,
				   .baseArrayLayer = 0,
				   .layerCount = 1} };
		vk::DependencyInfo depsInfo = {
			.dependencyFlags = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier };
		commandBuffer.pipelineBarrier2(depsInfo);

		// Update the current image layout, access mask, and stage mask
		m_CurrentLayout = params.Layout;
		m_CurrentAccessMask = params.AccessMask;
		m_CurrentStageMask = params.StageMask;
	}

	void Image::SetState(vk::ImageLayout layout, vk::AccessFlags2 accessMask, vk::PipelineStageFlags2 stageMask)
	{
		m_CurrentLayout = layout;
		m_CurrentAccessMask = accessMask;
		m_CurrentStageMask = stageMask;
	}

}