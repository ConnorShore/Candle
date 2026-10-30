#pragma once

#include "Candle/Render/RenderSpecification.h"
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
	// Threading: created and destroyed on the main thread; immutable in between, so any thread may read it
	class RenderInstance
	{
	public:
		RenderInstance(RenderInstanceSpecification spec);
		~RenderInstance() = default;

		// Non-copyable
		RenderInstance(const RenderInstance&) = delete;
		RenderInstance& operator=(const RenderInstance&) = delete;

		vk::raii::Instance& GetVulkanInstance() { return m_Instance; }
		inline bool IsDebugUtilsEnabled() const { return m_DebugUtilsEnabled; }

	private:
		void CreateInstance();
		void SetupDebugMessenger();

	private:
		RenderInstanceSpecification m_Specification;
		bool m_DebugUtilsEnabled{ false };

		vk::raii::Context m_Context{ };
		vk::raii::Instance m_Instance{ nullptr };
		vk::raii::DebugUtilsMessengerEXT m_DebugMessenger{ nullptr };
	};

}