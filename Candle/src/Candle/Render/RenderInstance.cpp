#include "cdlpch.h"
#include "RenderInstance.h"

#include "Candle/Core/Version.h"
#include "Candle/Platform/Platform.h"

#include <vector>

namespace Candle {

	namespace {

		const std::array<const char*, 1> validationLayers = {
			"VK_LAYER_KHRONOS_validation"
		};

		vk::DebugUtilsMessageSeverityFlagsEXT MapValidationSeverity(ValidationSpecification::ValidationSeverity severity)
		{
			vk::DebugUtilsMessageSeverityFlagsEXT flags = {};
			if (static_cast<uint8_t>(severity) & static_cast<uint8_t>(ValidationSpecification::ValidationSeverity::Verbose))
				flags |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
			if (static_cast<uint8_t>(severity) & static_cast<uint8_t>(ValidationSpecification::ValidationSeverity::Info))
				flags |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
			if (static_cast<uint8_t>(severity) & static_cast<uint8_t>(ValidationSpecification::ValidationSeverity::Warning))
				flags |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
			if (static_cast<uint8_t>(severity) & static_cast<uint8_t>(ValidationSpecification::ValidationSeverity::Error))
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
			std::string severity;
			if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose)
				severity = "Verbose";
			else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
				severity = "Info";
			else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
				severity = "Warning";
			else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
				severity = "Error";

			std::string type;
			if (messageType & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral)
				type = "General";
			else if (messageType & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
				type = "Validation";
			else if (messageType & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
				type = "Performance";

			CDL_CORE_ERROR(LogChannel::Render, "[Vulkan Debug] Severity: {}, Type: {}, Message: {}", severity, type, pCallbackData->pMessage);
			return VK_FALSE;
		}
	}

	RenderInstance::RenderInstance(RenderInstanceSpecification spec)
		: m_Specification(spec)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "RenderInstance must be created on the main thread!");
		CreateInstance();

		if (spec.ValidationSpec.EnableValidation)
			SetupDebugMessenger();
	}


	void RenderInstance::CreateInstance()
	{
		uint32_t vulkanVersion = m_Specification.VulkanVersion == VulkanAPIVersion::API_1_3 ? VK_API_VERSION_1_3 : VK_API_VERSION_1_4;
		vk::ApplicationInfo appInfo{ .pApplicationName = m_Specification.ApplicationName.data(),
											.applicationVersion = m_Specification.ApplicationVersion,
											.pEngineName = "Candle Engine",
											.engineVersion = kCandleVersion,
											.apiVersion = vulkanVersion };

		// Get the required validation layers
		int layerCount = m_Specification.ValidationSpec.EnableValidation ? static_cast<int>(validationLayers.size()) : 0;
		std::vector<char const*> requiredLayers;
		requiredLayers.reserve(layerCount);

		if (m_Specification.ValidationSpec.EnableValidation)
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());

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

		bool allExtensionsSupported = true;
		for (size_t i = 0; i < extensionCount; ++i)
		{
			bool extensionSupported = std::ranges::any_of(extensionProperties,
				[requiredExtension = requiredExtensions[i]](auto const& extensionProperty) {
					return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
				});
			if (!extensionSupported)
				throw std::runtime_error("Required extension not supported: " + *requiredExtensions[i]);
		}

		// This struct tells the Vulkan driver which global extensions and validation layers we want to use
		vk::InstanceCreateInfo createInfo{
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
			.ppEnabledLayerNames = requiredLayers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
			.ppEnabledExtensionNames = requiredExtensions.data()
		};

		// Create the Vulkan instance using the RAII wrapper. The instance will be automatically destroyed when it goes out of scope.
		m_Instance = vk::raii::Instance(m_Context, createInfo);
	}

	void RenderInstance::SetupDebugMessenger()
	{
		vk::DebugUtilsMessengerCreateInfoEXT createInfo{
			.messageSeverity = MapValidationSeverity(m_Specification.ValidationSpec.Severity),
			.messageType = MapValidationType(m_Specification.ValidationSpec.Type),
			.pfnUserCallback = &VulkanDebugCallback
		};
		m_DebugMessenger = vk::raii::DebugUtilsMessengerEXT(m_Instance, createInfo);
	}

}