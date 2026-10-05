#pragma once

#include "RenderSpecification.h"

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	struct RenderInstanceSpecification
	{
		bool EnableValidation = true;
		VulkanAPIVersion VulkanVersion = VulkanAPIVersion::API_1_4;

		const char* ApplicationName = "Candle App";

		uint32_t ApplicationVersion = 0;
	};

	// Holds the vulkan instance, context and debug messenger
	// Created and destroyed on the main thread (immutable)
	class RenderInstance
	{
	public:
		RenderInstance(RenderInstanceSpecification spec);
		~RenderInstance() = default;

		// Non-copyable
		RenderInstance(const RenderInstance&) = delete;
		RenderInstance& operator=(const RenderInstance&) = delete;

	private:
		void CreateInstance();

	private:
		RenderInstanceSpecification m_Specification;

		vk::raii::Context m_Context{ };
		vk::raii::Instance m_Instance{ nullptr };
	};

}