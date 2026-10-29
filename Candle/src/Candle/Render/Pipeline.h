#pragma once

#include "PipelineSpecification.h"

#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"

#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <type_traits>

namespace Candle {

	class RenderDevice;
	class BindlessTable;
	class SwapChain;

	// The one push-constant range every pipeline layout declares, so pushed values survive pipeline binds.
	// All stages (compute included, once it shares layouts); 128 bytes is the maxPushConstantsSize every device guarantees.
	inline constexpr vk::ShaderStageFlags kPushConstantStages = vk::ShaderStageFlagBits::eAll;
	inline constexpr uint32_t kPushConstantsSize = 128;

	// Threading: immutable once created, so the render thread may bind it after creation on another thread
	class Pipeline
	{
	public:
		Pipeline(const PipelineSpecification& spec, RenderDevice& renderDevice, BindlessTable& bindlessTable);
		~Pipeline() = default;

		// Delete copy constructor and assignment operator
		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;

		// Allow move constructor and assignment operator
		Pipeline(Pipeline&&) = default;
		Pipeline& operator=(Pipeline&&) = default;

		inline vk::raii::PipelineLayout& GetLayout() { return m_Layout; }
		inline vk::raii::Pipeline& GetPipeline() { return m_Pipeline; }

		// T is the pipeline's interop push struct; this is the C++ half of the contract ValidatePushConstants checks on the shader
		template<typename T>
		void PushConstants(vk::CommandBuffer commandBuffer, const T& data) const
		{
			CDL_STATIC_ASSERT(std::is_same_v<decltype(T::Global), GPU::GlobalPushConstants> && offsetof(T, Global) == 0,
				"A push-constant struct must start with 'GlobalPushConstants Global'");
			CDL_STATIC_ASSERT(sizeof(T) <= kPushConstantsSize, "Push-constant struct exceeds the engine-wide range");
			commandBuffer.pushConstants(*m_Layout, kPushConstantStages, 0, sizeof(T), &data);
		}

	private:
		void CreatePipeline(const PipelineSpecification& spec, RenderDevice& renderDevice, BindlessTable& bindlessTable);

	private:
		vk::raii::PipelineLayout m_Layout{ nullptr };
		vk::raii::Pipeline m_Pipeline{ nullptr };
	};

}