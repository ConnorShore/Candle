#pragma once

#include "Candle/Render/Resources/GPUHandles.h"
#include "Candle/Render/Vulkan/GPUImage.h"
#include "Candle/Render/Resources/GPUResourcePool.h"
#include "Candle/Render/Assets/TextureData.h"

#include "Candle/Render/Vulkan/VulkanUtils.h"

#include <glm/glm.hpp>

#include <array>
#include <span>
#include <string>
#include <vector>
#include <cstdint>

namespace Candle {

	class GPUAllocator;
	class GPUUploader;
	class GPUResourceRegistry;
	class BindlessTable;

	// Values, not references into the pool, so an info stays safe to hold after its texture is released
	struct TextureInfo
	{
		uint32_t Index{ std::numeric_limits<uint32_t>::max() };
		vk::ImageView ImageView{ nullptr };
		vk::Image Image{ nullptr };		// For barriers, when the renderer draws into a render target
		vk::Format Format{ vk::Format::eUndefined };
		glm::uvec2 Extent{ 0, 0 };
		uint32_t SizeBytes{ 0 };
		uint8_t MipLevels{ 1 };
		vk::ImageLayout SampledLayout{ vk::ImageLayout::eShaderReadOnlyOptimal };	// What its bindless descriptor expects, so it must be in this whenever it's sampled
		vk::ImageUsageFlags Usage{};	// eColorAttachment is what lets the scene draw into it
	};

	struct TextureCreateSpec
	{
		std::span<const std::byte> Data;
		glm::uvec2 Extent;
		TextureFormat Format;
		TextureColorSpace ColorSpace;
		std::string DebugName;
	};

	struct RenderTargetCreateSpec
	{
		glm::uvec2 Extent;
		vk::Format Format;	// Raw rather than TextureFormat, since it must match what the pipelines drawing into it target
		vk::ImageUsageFlags Usage{ vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled };	// eStorage keeps it in General for good
		std::string DebugName;
	};

	struct TextureStats
	{
		GPUTextureHandle Handle;
		std::string DebugName;
		uint32_t SizeBytes{ 0 };
		glm::uvec2 Extent{ 0, 0 };
		uint8_t MipLevels{ 1 };
		vk::Format Format{ vk::Format::eUndefined };	// Raw, since render targets have formats TextureFormat can't name
		vk::ImageUsageFlags Usage{};
	};

	struct TexturePoolStats
	{
		GPUResourcePoolStats PoolStats;
		std::vector<TextureStats> ActiveTextures{};
	};

	// Threading: CreateTexture, CreateRenderTarget, ReleaseTexture, IsAlive and CaptureStats on the main thread. GetTextureInfo on the main thread for a live
	//   handle, or on the render thread for a handle in the packet it is rendering, which stays valid until that frame retires.
	//   GetDefaultTexture from any thread, since the defaults never change after construction.
	// A released texture keeps its image until the GPU retires the frame that carried the release.
	class TexturePool
	{
	public:
		TexturePool(GPUAllocator& allocator, GPUUploader& uploader, GPUResourceRegistry& registry, BindlessTable& bindlessTable);
		~TexturePool() = default;

		// Delete copy constructor and assignment operator
		TexturePool(const TexturePool&) = delete;
		TexturePool& operator=(const TexturePool&) = delete;

		// Only reads data: it is copied into a staging buffer before this returns, so the caller keeps ownership.
		// Also writes the texture into the bindless table at its handle's index.
		GPUTextureHandle CreateTexture(TextureCreateSpec spec);

		// A texture the renderer draws or dispatches into and shaders then sample. Nothing is uploaded, so it must not be
		// sampled before a frame renders into it. Published at its handle's slot of each binding its usage allows.
		// Released like any other texture.
		GPUTextureHandle CreateRenderTarget(RenderTargetCreateSpec spec);
		void ReleaseTexture(GPUTextureHandle handle);

		// False for a value-initialized handle and for a released texture, even once another texture reuses its slot.
		// True for the default textures, which are never released.
		bool IsAlive(GPUTextureHandle handle) const;

		// The default handle resolves to DefaultTexture::Error; a stale one asserts, and resolves to it too outside Debug
		TextureInfo GetTextureInfo(GPUTextureHandle handle) const;
		TexturePoolStats CaptureStats() const;

		// Created and written into the bindless table with the pool, and never released
		inline GPUTextureHandle GetDefaultTexture(DefaultTexture texture) const { return m_DefaultTextures[static_cast<size_t>(texture)]; }

		inline GPUResourcePoolStats CapturePoolStats() const { return m_Textures.CaptureStats(); }

	private:
		struct TextureEntry
		{
			GPUImage Image;
			glm::uvec2 Extent;
			vk::ImageLayout SampledLayout{ vk::ImageLayout::eShaderReadOnlyOptimal };

			uint64_t GetSizeBytes() const { return Image.GetSizeBytes(); }
		};

	private:
		void StoreDebugName(GPUTextureHandle handle, std::string name);

	private:
		GPUAllocator& m_Allocator;
		GPUUploader& m_Uploader;
		BindlessTable& m_BindlessTable;
		GPUResourcePool<TextureEntry, TextureTag> m_Textures;
		std::vector<std::string> m_DebugNames;	// Main thread only, indexed by slot. Kept out of the entries since the render thread never needs them, so it can grow.
		std::array<GPUTextureHandle, static_cast<size_t>(DefaultTexture::Count)> m_DefaultTextures;	// After m_Textures, which the constructor creates them in
	};

}