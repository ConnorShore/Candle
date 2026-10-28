#include "cdlpch.h"
#include "BindlessTable.h"
#include "RenderDevice.h"
#include "RenderConstants.h"
#include "TexturePool.h"

#include "Candle/Platform/Platform.h"

namespace Candle {

	namespace {

		// The spec's minimum maxSamplerAnisotropy on any device with samplerAnisotropy, which RenderDevice requires
		constexpr float kMaxAnisotropy = 16.0f;

		constexpr vk::SamplerCreateInfo MakeSamplerInfo(vk::Filter filter, vk::SamplerAddressMode addressMode, bool anisotropic)
		{
			return vk::SamplerCreateInfo{
				.magFilter = filter,
				.minFilter = filter,
				.mipmapMode = filter == vk::Filter::eLinear ? vk::SamplerMipmapMode::eLinear : vk::SamplerMipmapMode::eNearest,
				.addressModeU = addressMode,
				.addressModeV = addressMode,
				.addressModeW = addressMode,
				.anisotropyEnable = anisotropic,
				.maxAnisotropy = anisotropic ? kMaxAnisotropy : 1.0f,
				.maxLod = vk::LodClampNone,	// Every mip the image has, so a sampler never caps a texture's chain
			};
		}

		// Only LinearWrap is anisotropic: the others serve screen-aligned or exact-texel reads, never an oblique footprint
		constexpr vk::SamplerCreateInfo GetSamplerInfo(SamplerType type)
		{
			switch (type)
			{
				case SamplerType::LinearWrap:  return MakeSamplerInfo(vk::Filter::eLinear,  vk::SamplerAddressMode::eRepeat,      true);
				case SamplerType::LinearClamp: return MakeSamplerInfo(vk::Filter::eLinear,  vk::SamplerAddressMode::eClampToEdge, false);
				case SamplerType::PointWrap:   return MakeSamplerInfo(vk::Filter::eNearest, vk::SamplerAddressMode::eRepeat,      false);
				case SamplerType::PointClamp:  return MakeSamplerInfo(vk::Filter::eNearest, vk::SamplerAddressMode::eClampToEdge, false);
				default: break;
			}
			std::unreachable();	// Not constexpr, so a SamplerType with no case fails kSamplerInfos' constant evaluation
		}

		// Indexed by SamplerType, and built from the switch so the order cannot drift from the enum
		constexpr auto kSamplerInfos = [] {
			std::array<vk::SamplerCreateInfo, static_cast<size_t>(SamplerType::Count)> infos{};
			for (size_t i = 0; i < infos.size(); ++i)
				infos[i] = GetSamplerInfo(static_cast<SamplerType>(i));
			return infos;
		}();
	}

	BindlessTable::BindlessTable(RenderDevice& renderDevice)
		: m_RenderDevice(renderDevice)
	{
		CreateSamplers(renderDevice);	// First: the layout sizes binding 1 from m_Samplers
		CreateDescriptorPool(renderDevice);
		CreateDescriptorSetLayout(renderDevice);
		CreateDescriptorSet(renderDevice);
	}

	void BindlessTable::CreateDescriptorPool(RenderDevice& renderDevice)
	{
		std::array poolSize{
			vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, kMaxTextures),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampler, static_cast<uint32_t>(SamplerType::Count)) 
		};
		vk::DescriptorPoolCreateInfo poolInfo{
			// FreeDescriptorSet because vk::raii::DescriptorSet frees itself on destruction
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
			.maxSets = 1,	// One set holds both bindings
			.poolSizeCount = static_cast<uint32_t>(poolSize.size()),
			.pPoolSizes = poolSize.data() 
		};
		m_DescriptorPool = vk::raii::DescriptorPool(renderDevice.GetDevice(), poolInfo);
	}

	void BindlessTable::CreateDescriptorSetLayout(RenderDevice& renderDevice)
	{
		uint32_t samplerSize = static_cast<uint32_t>(m_Samplers.size());
		std::array material_bindings = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eSampledImage, static_cast<uint32_t>(kMaxTextures), vk::ShaderStageFlagBits::eFragment, nullptr),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eSampler, samplerSize, vk::ShaderStageFlagBits::eFragment, nullptr) 
		};

		std::vector<vk::DescriptorBindingFlags> bindingFlags = {
			// Unwritten or retired elements are legal while no shader indexes them, and may be written while other elements are in flight
			vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending,
			vk::DescriptorBindingFlagBits::eUpdateAfterBind 
		};

		vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsCreateInfo{
			.bindingCount = static_cast<uint32_t>(bindingFlags.size()),
			.pBindingFlags = bindingFlags.data() 
		};

		vk::DescriptorSetLayoutCreateInfo layoutInfo{
			.pNext = &flagsCreateInfo,
			.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
			.bindingCount = static_cast<uint32_t>(material_bindings.size()),
			.pBindings = material_bindings.data(),
		};

		m_DescriptorSetLayout = vk::raii::DescriptorSetLayout(renderDevice.GetDevice(), layoutInfo);
	}

	void BindlessTable::CreateDescriptorSet(RenderDevice& renderDevice)
	{
		std::vector<vk::DescriptorSetLayout> layouts{ *m_DescriptorSetLayout };

		vk::DescriptorSetAllocateInfo allocInfo{
			.descriptorPool = m_DescriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
			.pSetLayouts = layouts.data() };

		m_DescriptorSet = std::move(vk::raii::DescriptorSets(renderDevice.GetDevice(), allocInfo).front());

		// Textures start unwritten, which PartiallyBound allows; TexturePool writes each one through WriteTexture

		// Samplers
		std::vector<vk::DescriptorImageInfo> samplerInfos;
		samplerInfos.reserve(m_Samplers.size());
		for (auto& sampler : m_Samplers)
		{
			vk::DescriptorImageInfo samplerInfo{
				.sampler = *sampler,
				.imageView = nullptr,
				.imageLayout = vk::ImageLayout::eUndefined };
			samplerInfos.push_back(samplerInfo);
		}

		vk::WriteDescriptorSet samplerWrite{
			.dstSet = *m_DescriptorSet,
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = static_cast<uint32_t>(samplerInfos.size()),
			.descriptorType = vk::DescriptorType::eSampler,
			.pImageInfo = samplerInfos.data()
		};

		renderDevice.GetDevice().updateDescriptorSets({ samplerWrite }, {});
	}

	void BindlessTable::WriteTexture(uint32_t index, vk::ImageView view)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "BindlessTable textures are written on the main thread");
		CDL_CORE_ASSERT(index < kMaxTextures, "Texture index is outside the bindless array");

		vk::DescriptorImageInfo imageInfo{
			.imageView = view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };	// The layout when sampled, not now: the upload may not have run yet

		vk::WriteDescriptorSet textureWrite{
			.dstSet = *m_DescriptorSet,
			.dstBinding = 0,
			.dstArrayElement = index,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eSampledImage,
			.pImageInfo = &imageInfo
		};

		m_RenderDevice.GetDevice().updateDescriptorSets({ textureWrite }, {});
	}

	void BindlessTable::WriteDefaultTextures(const TexturePool& pool)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(DefaultTexture::Count); ++i)
		{
			const TextureInfo info = pool.GetTextureInfo(pool.GetDefaultTexture(static_cast<DefaultTexture>(i)));
			WriteTexture(info.Index, info.ImageView);
		}
	}

	void BindlessTable::BindSet(vk::CommandBuffer commandBuffer, vk::PipelineBindPoint bindPoint, vk::PipelineLayout& pipelineLayout)
	{
		commandBuffer.bindDescriptorSets(bindPoint, pipelineLayout, 0, { *m_DescriptorSet }, {});
	}

	void BindlessTable::CreateSamplers(RenderDevice& renderDevice)
	{
		m_Samplers.reserve(kSamplerInfos.size());
		for (const vk::SamplerCreateInfo& samplerInfo : kSamplerInfos)
			m_Samplers.emplace_back(renderDevice.GetDevice(), samplerInfo);
	}

}