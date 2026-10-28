#include "cdlpch.h"
#include "TexturePool.h"
#include "GPUAllocator.h"
#include "GPUUploader.h"
#include "GPUResourceRegistry.h"

#include "VulkanUtils.h"

namespace Candle {

	TexturePool::TexturePool(GPUAllocator& allocator, GPUUploader& uploader, GPUResourceRegistry& registry)
		: m_Allocator(allocator), m_Uploader(uploader), m_Textures(registry, kMaxTextures)
	{
	}

	GPUTextureHandle TexturePool::CreateTexture(TextureCreateSpec spec)
	{
		CDL_CORE_ASSERT(spec.Extent.x > 0 && spec.Extent.y > 0, "A texture must have an area > 0");
		CDL_CORE_ASSERT(spec.Data.size_bytes() == static_cast<size_t>(spec.Extent.x) * spec.Extent.y * BytesPerTexel(spec.Format),
			"Texture data must be Extent.x * Extent.y tightly packed texels of its format");

		GPUImage image = m_Allocator.CreateImage(GPUImageSpec{
			.m_Extent = spec.Extent,
			.m_Format = ToVulkanFormat(spec.Format, spec.ColorSpace),
			});

		ImageUploadSpec uploadSpec{
			.Data = spec.Data,
			.Destination = image.GetImage(),
			.DestinationLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.Extent = spec.Extent
		};
		
		m_Uploader.UploadImage(uploadSpec,
			vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eFragmentShader,
			vk::AccessFlagBits2::eShaderSampledRead);

		return m_Textures.Insert(TextureEntry{ .Image = std::move(image) });
	}

	void TexturePool::ReleaseTexture(GPUTextureHandle handle)
	{
		m_Textures.Release(handle);
	}

	bool TexturePool::IsAlive(GPUTextureHandle handle) const
	{
		return m_Textures.IsAlive(handle);
	}

	TextureInfo TexturePool::GetTextureInfo(GPUTextureHandle handle) const
	{
		CDL_CORE_ASSERT(IsAlive(handle), "GetTextureInfo was given a dead texture handle");
		if (!IsAlive(handle))
			return {};

		return TextureInfo{
			.Index = handle.Index,
			.ImageView = m_Textures.Get(handle).Image.GetImageView()
		};
	}

}