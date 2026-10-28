#pragma once

#include "GPUHandles.h"
#include "GPUImage.h"
#include "GPUResourcePool.h"
#include "TextureData.h"

#include <array>
#include <span>

namespace Candle {

	class GPUAllocator;
	class GPUUploader;
	class GPUResourceRegistry;

	// Values, not references into the pool, so an info stays safe to hold after its texture is released
	struct TextureInfo
	{
		uint32_t Index{ std::numeric_limits<uint32_t>::max() };
		vk::ImageView ImageView{ nullptr };
	};

	struct TextureCreateSpec
	{
		std::span<const std::byte> Data;
		glm::uvec2 Extent;
		TextureFormat Format;
		TextureColorSpace ColorSpace;
	};

	// Threading: CreateTexture, ReleaseTexture and IsAlive on the main thread. GetTextureInfo on the main thread for a live
	//   handle, or on the render thread for a handle in the packet it is rendering, which stays valid until that frame retires.
	//   GetDefaultTexture from any thread, since the defaults never change after construction.
	// A released texture keeps its image until the GPU retires the frame that carried the release.
	class TexturePool
	{
	public:
		TexturePool(GPUAllocator& allocator, GPUUploader& uploader, GPUResourceRegistry& registry);
		~TexturePool() = default;

		// Delete copy constructor and assignment operator
		TexturePool(const TexturePool&) = delete;
		TexturePool& operator=(const TexturePool&) = delete;

		// Only reads data: it is copied into a staging buffer before this returns, so the caller keeps ownership
		GPUTextureHandle CreateTexture(TextureCreateSpec spec);
		void ReleaseTexture(GPUTextureHandle handle);

		// False for the default handle and for a released mesh, even once another mesh reuses its slot
		bool IsAlive(GPUTextureHandle handle) const;

		// The default handle resolves to DefaultTexture::Error; a stale one asserts, and resolves to it too outside Debug
		TextureInfo GetTextureInfo(GPUTextureHandle handle) const;

		// Created with the pool and never released; BindlessTable::WriteDefaultTextures must publish them before any frame
		inline GPUTextureHandle GetDefaultTexture(DefaultTexture texture) const { return m_DefaultTextures[static_cast<size_t>(texture)]; }

	private:
		struct TextureEntry
		{
			GPUImage Image;
		};

	private:
		GPUAllocator& m_Allocator;
		GPUUploader& m_Uploader;
		GPUResourcePool<TextureEntry, TextureTag> m_Textures;
		std::array<GPUTextureHandle, static_cast<size_t>(DefaultTexture::Count)> m_DefaultTextures;	// After m_Textures, which the constructor creates them in
	};

}