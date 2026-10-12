#pragma once

#include "PipelineSpecification.h"

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	class RenderDevice;
	class SwapChain;

	class Pipeline
	{
	public:
		Pipeline(PipelineSpecification spec, RenderDevice& renderDevice, vk::Format format);
		~Pipeline() = default;

		// Delete copy constructor and assignment operator
		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;

		// Allow move constructor and assignment operator
		Pipeline(Pipeline&&) = default;
		Pipeline& operator=(Pipeline&&) = default;

		inline vk::raii::PipelineLayout& GetLayout() { return m_Layout; }
		inline vk::raii::Pipeline& GetPipeline() { return m_Pipeline; }

	private:
		void CreatePipeline(vk::Format format, RenderDevice& renderDevice);

	private:
		PipelineSpecification m_Specification;

		vk::raii::PipelineLayout m_Layout{ nullptr };
		vk::raii::Pipeline m_Pipeline{ nullptr };
	};

}