#pragma once

#include "RenderSpecification.h"
#include "Candle/Core/Version.h"

#include <vulkan/vulkan_raii.hpp>

#include <string>

namespace Candle {

	struct RenderInstanceSpecification
	{
		VulkanAPIVersion VulkanVersion = VulkanAPIVersion::API_1_4;
		ValidationSpecification ValidationSpec = { };

		std::string ApplicationName = "Candle App";
		VersionInfo ApplicationVersion = { 1, 0, 0 };
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

		vk::raii::Instance& GetVulkanInstance() { return m_Instance; }

	private:
		void CreateInstance();
		void SetupDebugMessenger();

	private:
		RenderInstanceSpecification m_Specification;

		vk::raii::Context m_Context{ };
		vk::raii::Instance m_Instance{ nullptr };
		vk::raii::DebugUtilsMessengerEXT m_DebugMessenger{ nullptr };
	};

}