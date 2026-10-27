#pragma once

#include "GPUHandles.h"
#include "GPUImage.h"
#include "GPUResourcePool.h"
#include "TextureData.h"

#include <span>

namespace Candle {

	class GPUAllocator;
	class GPUUploader;
	class GPUResourceRegistry;

	struct TextureInfo
	{
		uint32_t Index;
	};

	struct TextureCreateSpec
	{
		std::span<const std::byte> Data;
		glm::uvec2 Extent;
		TextureFormat Format;
		TextureColorSpace ColorSpace;
	};

	class TexturePool
	{
	public:
		TexturePool(GPUAllocator& allocator, GPUUploader& uploader, GPUResourceRegistry& registry);
		~TexturePool() = default;

		// Delete copy constructor and assignment operator
		TexturePool(const TexturePool&) = delete;
		TexturePool& operator=(const TexturePool&) = delete;

		// TODO: Maybe have own Format enum
		// Only reads data: it is copied into a staging buffer before this returns, so the caller keeps ownership
		GPUTextureHandle CreateTexture(TextureCreateSpec spec);
		void ReleaseTexture(GPUTextureHandle handle);

		// False for the default handle and for a released mesh, even once another mesh reuses its slot
		bool IsAlive(GPUTextureHandle handle) const;

		TextureInfo GetTextureInfo(GPUTextureHandle handle) const;

	private:
		struct TextureEntry
		{
			GPUImage Image;
		};

	private:
		GPUAllocator& m_Allocator;
		GPUUploader& m_Uploader;
		GPUResourcePool<TextureEntry, TextureTag> m_Textures;
	};

}