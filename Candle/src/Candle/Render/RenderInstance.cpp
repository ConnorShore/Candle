#include "cdlpch.h"
#include "RenderInstance.h"
#include "VulkanUtils.h"

#include "Candle/Core/Version.h"
#include "Candle/Platform/Platform.h"

#include <vector>

namespace Candle {

	namespace {

		constexpr std::array<const char*, 1> kValidationLayers = {
			"VK_LAYER_KHRONOS_validation"
		};

		vk::DebugUtilsMessageSeverityFlagsEXT MapValidationSeverity(ValidationSpecification::ValidationSeverity minimum)
		{
			using Severity = ValidationSpecification::ValidationSeverity;

			vk::DebugUtilsMessageSeverityFlagsEXT flags = {};
			if (minimum <= Severity::Verbose)
				flags |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
			if (minimum <= Severity::Info)
				flags |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
			if (minimum <= Severity::Warning)
				flags |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
			flags |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
			return flags;
		}

		vk::DebugUtilsMessageTypeFlagsEXT MapValidationType(ValidationSpecification::ValidationType type)
		{
			vk::DebugUtilsMessageTypeFlagsEXT flags = {};
			if (static_cast<uint8_t>(type) & static_cast<uint8_t>(ValidationSpecification::ValidationType::General))
				flags |= vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral;
			if (static_cast<uint8_t>(type) & static_cast<uint8_t>(ValidationSpecification::ValidationType::Validation))
				flags |= vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
			if (static_cast<uint8_t>(type) & static_cast<uint8_t>(ValidationSpecification::ValidationType::Performance))
				flags |= vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
			return flags;
		}

		VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanDebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
																vk::DebugUtilsMessageTypeFlagsEXT messageType,
																const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
																void* pUserData)
		{
			std::string type;
			if (messageType & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral)
				type = "General";
			else if (messageType & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
				type = "Validation";
			else if (messageType & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
				type = "Performance";

			switch (messageSeverity)
			{
			case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
				CDL_CORE_TRACE(LogChannel::Render, "[Vulkan Debug] Severity: Verbose, Type: {}, Message: {}", type, pCallbackData->pMessage);
				break;
			case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
				CDL_CORE_INFO(LogChannel::Render, "[Vulkan Debug] Severity: Info, Type: {}, Message: {}", type, pCallbackData->pMessage);
				break;
			case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
				CDL_CORE_WARN(LogChannel::Render, "[Vulkan Debug] Severity: Warning, Type: {}, Message: {}", type, pCallbackData->pMessage);
				break;
			case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
				CDL_CORE_ERROR(LogChannel::Render, "[Vulkan Debug] Severity: Error, Type: {}, Message: {}", type, pCallbackData->pMessage);
				break;
			default:
				CDL_CORE_ERROR(LogChannel::Render, "[Vulkan Debug] Unknown severity level. Type: {}, Message: {}", type, pCallbackData->pMessage);
				break;
			}

			return VK_FALSE;
		}
	}

	RenderInstance::RenderInstance(RenderInstanceSpecification spec)
		: m_Specification(spec)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "RenderInstance must be created on the main thread!");
		CreateInstance();
		SetupDebugMessenger();
	}

	void RenderInstance::CreateInstance()
	{
		vk::ApplicationInfo appInfo{ .pApplicationName = m_Specification.ApplicationName.data(),
											.applicationVersion = ToVulkanVersion(m_Specification.ApplicationVersion),
											.pEngineName = "Candle Engine",
											.engineVersion = ToVulkanVersion(kCandleVersion),
											.apiVersion = ToVulkanApiVersion(m_Specification.VulkanVersion) };

		// Get the required validation layers
		int layerCount = m_Specification.ValidationSpec.EnableValidation ? static_cast<int>(kValidationLayers.size()) : 0;
		std::vector<char const*> requiredLayers;
		requiredLayers.reserve(layerCount);

		if (m_Specification.ValidationSpec.EnableValidation)
			requiredLayers.assign(kValidationLayers.begin(), kValidationLayers.end());

		// Check if the required layers are supported by the Vulkan implementation.
		auto layerProperties = m_Context.enumerateInstanceLayerProperties();
		auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
			[&layerProperties](auto const& requiredLayer) {
				return std::ranges::none_of(layerProperties,
					[requiredLayer](auto const& layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
			});
		if (unsupportedLayerIt != requiredLayers.end())
			throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));

		// Get the required instance extensions
		uint32_t extensionCount = 0;
		auto platformRequiredExtensions = Platform::GetVulkanRequiredInstanceExtensions(extensionCount);
		std::vector<char const*> requiredExtensions(platformRequiredExtensions, platformRequiredExtensions + extensionCount);

		if (m_Specification.ValidationSpec.EnableValidation)
			requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);

		auto extensionProperties = m_Context.enumerateInstanceExtensionProperties();

		// Debug logging for now
		for (const auto& extension : extensionProperties) 
		{
			std::string extensionName = extension.extensionName;
			CDL_CORE_TRACE(LogChannel::Render, "Available Vulkan Extension: {}", extensionName);
		}

		// Every required extension, including debug utils, not just the platform's
		for (const char* requiredExtension : requiredExtensions)
		{
			bool extensionSupported = std::ranges::any_of(extensionProperties,
				[requiredExtension](auto const& extensionProperty) {
					return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
				});
			if (!extensionSupported)
				throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
		}

		vk::InstanceCreateInfo createInfo{
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
			.ppEnabledLayerNames = requiredLayers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
			.ppEnabledExtensionNames = requiredExtensions.data()
		};

		m_Instance = vk::raii::Instance(m_Context, createInfo);
	}

	void RenderInstance::SetupDebugMessenger()
	{
		if (!m_Specification.ValidationSpec.EnableValidation)
			return;

		vk::DebugUtilsMessengerCreateInfoEXT createInfo{
			.messageSeverity = MapValidationSeverity(m_Specification.ValidationSpec.Severity),
			.messageType = MapValidationType(m_Specification.ValidationSpec.Type),
			.pfnUserCallback = &VulkanDebugCallback
		};
		m_DebugMessenger = vk::raii::DebugUtilsMessengerEXT(m_Instance, createInfo);
	}

}