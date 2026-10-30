#include "cdlpch.h"
#include "TexturePool.h"
#include "Candle/Render/Vulkan/GPUAllocator.h"
#include "Candle/Render/Resources/GPUUploader.h"
#include "Candle/Render/Resources/GPUResourceRegistry.h"
#include "Candle/Render/Vulkan/BindlessTable.h"

#include "Candle/Render/Vulkan/VulkanUtils.h"

namespace Candle {

	namespace {

		using DefaultTexel = std::array<std::byte, 4>;	// One linear RGBA8 texel, so its bytes are exactly what a shader samples

		constexpr DefaultTexel GetDefaultTexel(DefaultTexture texture)
		{
			switch (texture)
			{
				case DefaultTexture::Error: return { std::byte{ 255 }, std::byte{ 0 }, std::byte{ 255 }, std::byte{ 255 } };	// No real texture is flat magenta
				default: break;
			}
			std::unreachable();	// Not constexpr, so a DefaultTexture with no case fails kDefaultTexels' constant evaluation
		}

		// Indexed by DefaultTexture, and built from the switch so the order cannot drift from the enum
		constexpr auto kDefaultTexels = [] {
			std::array<DefaultTexel, static_cast<size_t>(DefaultTexture::Count)> texels{};
			for (size_t i = 0; i < texels.size(); ++i)
				texels[i] = GetDefaultTexel(static_cast<DefaultTexture>(i));
			return texels;
		}();
	}

	TexturePool::TexturePool(GPUAllocator& allocator, GPUUploader& uploader, GPUResourceRegistry& registry, BindlessTable& bindlessTable)
		: m_Allocator(allocator), m_Uploader(uploader), m_BindlessTable(bindlessTable), m_Textures(registry, kMaxTextures)
	{
		for (size_t i = 0; i < kDefaultTexels.size(); ++i)
		{
			m_DefaultTextures[i] = CreateTexture({
				.Data = kDefaultTexels[i], .Extent = { 1, 1 }, .Format = TextureFormat::Rgba8, .ColorSpace = TextureColorSpace::Linear,
				.DebugName = std::format("Default Texture {}", i) });

			// A fresh pool hands out slots in order, which is what makes each default's bindless index its enum value
			CDL_CORE_ASSERT(m_DefaultTextures[i].Index == i, "Default textures must occupy the pool's first slots");
		}
	}

	GPUTextureHandle TexturePool::CreateTexture(TextureCreateSpec spec)
	{
		CDL_CORE_ASSERT(spec.Extent.x > 0 && spec.Extent.y > 0, "A texture must have an area > 0");
		CDL_CORE_ASSERT(spec.Data.size_bytes() == static_cast<size_t>(spec.Extent.x) * spec.Extent.y * BytesPerTexel(spec.Format),
			"Texture data must be Extent.x * Extent.y tightly packed texels of its format");

		GPUImage image = m_Allocator.CreateImage(GPUImageSpec{
			.Extent = spec.Extent,
			.Format = ToVulkanFormat(spec.Format, spec.ColorSpace),
			.DebugName = spec.DebugName
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

		vk::ImageView view = image.GetImageView();
		GPUTextureHandle handle = m_Textures.Insert(TextureEntry{ .Image = std::move(image) });

		// Published here so no texture can be handed out before a shader can sample it
		m_BindlessTable.WriteTexture(handle.Index, view);
		return handle;
	}

	void TexturePool::ReleaseTexture(GPUTextureHandle handle)
	{
		// Failed lookups resolve to them and shaders may index them as constants, so they outlive every other texture
		const bool isDefault = handle.Index < static_cast<uint32_t>(DefaultTexture::Count);
		CDL_CORE_ASSERT(!isDefault, "Default textures are never released");
		if (isDefault)
			return;

		m_Textures.Release(handle);
	}

	bool TexturePool::IsAlive(GPUTextureHandle handle) const
	{
		return m_Textures.IsAlive(handle);
	}

	TextureInfo TexturePool::GetTextureInfo(GPUTextureHandle handle) const
	{
		// Not IsAlive: that is the main thread's view, and the render thread may still draw a texture released after its packet
		const TextureEntry* entry = m_Textures.TryGet(handle);
		CDL_CORE_ASSERT(entry || !handle.IsValid(), "GetTextureInfo was given a stale texture handle");
		if (!entry)
		{
			handle = GetDefaultTexture(DefaultTexture::Error);
			entry = &m_Textures.Get(handle);
		}

		return TextureInfo{
			.Index = handle.Index,
			.ImageView = entry->Image.GetImageView()
		};
	}

}