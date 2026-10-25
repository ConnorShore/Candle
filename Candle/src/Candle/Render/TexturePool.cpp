#include "cdlpch.h"
#include "TexturePool.h"
#include "GPUAllocator.h"
#include "GPUUploader.h"
#include "GPUResourceRegistry.h"

namespace Candle {



	TexturePool::TexturePool(GPUAllocator& allocator, GPUUploader& uploader, GPUResourceRegistry& registry)
		: m_Allocator(allocator), m_Uploader(uploader), m_Textures(registry, kMaxTextures)
	{
	}

	GPUTextureHandle TexturePool::CreateTexture(glm::uvec2 extent, vk::Format format)
	{
		CDL_CORE_ASSERT(extent.x > 0 && extent.y > 0, "A texture must have an area > 0");

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
		return TextureInfo{
			.Index = handle.Index
		};
	}

}