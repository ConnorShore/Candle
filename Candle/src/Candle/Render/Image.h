#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	struct ImageDesc
	{
		vk::Extent2D Extent;
		vk::Format	 Format;
	};
	
	struct ImageTransitionParams
	{
		vk::ImageLayout         Layout;
		vk::AccessFlags2        AccessMask;
		vk::PipelineStageFlags2 StageMask; 
		vk::ImageAspectFlags	AspectMask;
	};

	class Image
	{
	public:
		Image(vk::Image image, vk::raii::ImageView imageView, const ImageDesc& desc);
		~Image() = default;

		// Delete copy constructor and assignment operator
		Image(const Image&) = delete;
		Image& operator=(const Image&) = delete;

		// Allow move constructor and assignment operator
		Image(Image&&) = default;
		Image& operator=(Image&&) = default;

		void Transition(const ImageTransitionParams& params, vk::raii::CommandBuffer& commandBuffer);

		// Overwrites the tracked state without a barrier; for an image last used outside our command stream, e.g. by presentation
		void SetState(vk::ImageLayout layout, vk::AccessFlags2 accessMask, vk::PipelineStageFlags2 stageMask);

		inline vk::raii::ImageView& GetImageView() { return m_ImageView; }
		inline ImageDesc& GetDesc() { return m_Desc; }

	private:
		vk::Image m_Image;
		vk::raii::ImageView m_ImageView{ nullptr };

		ImageDesc m_Desc;

		vk::ImageLayout m_CurrentLayout{ vk::ImageLayout::eUndefined };
		vk::AccessFlags2 m_CurrentAccessMask{ vk::AccessFlagBits2::eColorAttachmentWrite };
		vk::PipelineStageFlags2 m_CurrentStageMask{ vk::PipelineStageFlagBits2::eColorAttachmentOutput };
	};

}