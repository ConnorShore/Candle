#pragma once

#include "SamplerType.h"

#include <vector>

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	class RenderDevice;
	class TexturePool;

	// Set 0 of every pipeline layout. Binding 0 is Texture2D[kMaxTextures], element i being TexturePool slot i;
	// binding 1 is SamplerState[SamplerType::Count], written once at construction.
	// Threading: construct, and WriteDefaultTextures, before the render thread starts. WriteTexture on the main thread while the render thread
	//   binds the set, which binding 0's UPDATE_AFTER_BIND permits. The caller must never write an element a pending
	//   command buffer may read; TexturePool guarantees it, since a slot is only reissued once the GPU has retired it.
	//   GetSet and GetLayout from any thread.
	class BindlessTable
	{
	public:
		BindlessTable(RenderDevice& renderDevice);
		~BindlessTable() = default;

		// Delete copy constructor and assignment operator
		BindlessTable(const BindlessTable&) = delete;
		BindlessTable& operator=(const BindlessTable&) = delete;

		void WriteTexture(uint32_t index, vk::ImageView view);
		void WriteDefaultTextures(const TexturePool& pool);	// Every DefaultTexture, at the element its value names
		void BindSet(vk::CommandBuffer commandBuffer, vk::PipelineBindPoint bindPoint, vk::PipelineLayout& pipelineLayout);

		inline vk::DescriptorSet GetSet() const { return *m_DescriptorSet; }
		inline vk::DescriptorSetLayout GetLayout() const { return *m_DescriptorSetLayout; }

	private:
		void CreateDescriptorPool(RenderDevice& renderDevice);
		void CreateDescriptorSetLayout(RenderDevice& renderDevice);
		void CreateDescriptorSet(RenderDevice& renderDevice);
		void CreateSamplers(RenderDevice& renderDevice);

	private:
		RenderDevice& m_RenderDevice;
		std::vector<vk::raii::Sampler> m_Samplers;
		vk::raii::DescriptorPool m_DescriptorPool{ nullptr };
		vk::raii::DescriptorSetLayout m_DescriptorSetLayout{ nullptr };
		vk::raii::DescriptorSet m_DescriptorSet{ nullptr };	// Declared after the pool, so it is freed while the pool still exists
	};

}