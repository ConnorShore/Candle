#pragma once

#include "Candle/Render/Vulkan/SamplerType.h"

#include <vector>

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	class RenderDevice;

	// Set 0 of every pipeline layout. Binding 0 is Texture2D[kMaxTextures], element i being TexturePool slot i;
	// binding 1 is SamplerState[SamplerType::Count], written once at construction; binding 2 is RWTexture2D[kMaxTextures],
	// indexed by the same slots and written only for images with storage usage.
	// Threading: construct before the render thread starts. WriteTexture and WriteStorageImage on the main thread while the
	//   render thread binds the set, which UPDATE_AFTER_BIND on bindings 0 and 2 permits. The caller must never write an
	//   element a pending command buffer may read; TexturePool guarantees it, since a slot is only reissued once the GPU has
	//   retired it. GetSet and GetLayout from any thread.
	class BindlessTable
	{
	public:
		BindlessTable(RenderDevice& renderDevice);
		~BindlessTable() = default;

		// Delete copy constructor and assignment operator
		BindlessTable(const BindlessTable&) = delete;
		BindlessTable& operator=(const BindlessTable&) = delete;

		// layout is the one the image is in whenever it's sampled, not its layout now
		void WriteTexture(uint32_t index, vk::ImageView view, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
		void WriteStorageImage(uint32_t index, vk::ImageView view);	// Storage images are always accessed in General
		void BindSet(vk::CommandBuffer commandBuffer, vk::PipelineBindPoint bindPoint, vk::PipelineLayout pipelineLayout);

		inline vk::DescriptorSet GetSet() const { return *m_DescriptorSet; }
		inline vk::DescriptorSetLayout GetLayout() const { return *m_DescriptorSetLayout; }

	private:
		void CreateDescriptorPool(RenderDevice& renderDevice);
		void CreateDescriptorSetLayout(RenderDevice& renderDevice);
		void CreateDescriptorSet(RenderDevice& renderDevice);
		void CreateSamplers(RenderDevice& renderDevice);

		void WriteImage(uint32_t binding, vk::DescriptorType type, uint32_t index, vk::ImageView view, vk::ImageLayout layout);

	private:
		RenderDevice& m_RenderDevice;
		std::vector<vk::raii::Sampler> m_Samplers;
		vk::raii::DescriptorPool m_DescriptorPool{ nullptr };
		vk::raii::DescriptorSetLayout m_DescriptorSetLayout{ nullptr };
		vk::raii::DescriptorSet m_DescriptorSet{ nullptr };	// Declared after the pool, so it is freed while the pool still exists
	};

}