#include "cdlpch.h"
#include "RenderInstance.h"
#include "VulkanUtils.h"

#include "Candle/Core/Version.h"
#include "Candle/Platform/Platform.h"

#include <vector>

namespace Candle {

	namespace {

		constexpr const char* kKhronosValidationLayer = "VK_LAYER_KHRONOS_validation";
		constexpr std::array<const char*, 1> kValidationLayers = {
			kKhronosValidationLayer
		};

		bool HasExtension(const std::vector<vk::ExtensionProperties>& extensions, const char* name)
		{
			return std::ranges::any_of(extensions, [name](const vk::ExtensionProperties& extension) { return strcmp(extension.extensionName, name) == 0; });
		}

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
			CDL_CORE_PANIC(LogChannel::Render, "Required layer not supported: {} (validation layers come with the Vulkan SDK)", *unsupportedLayerIt);

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
			if (!HasExtension(extensionProperties, requiredExtension))
				CDL_CORE_PANIC(LogChannel::Render, "Required extension not supported: {}", requiredExtension);
		}

		// Without validation, debug utils only names objects, so it's optional
		m_DebugUtilsEnabled = m_Specification.ValidationSpec.EnableValidation;
		if (!m_DebugUtilsEnabled && m_Specification.ValidationSpec.EnableDebugUtils)
		{
			m_DebugUtilsEnabled = HasExtension(extensionProperties, vk::EXTDebugUtilsExtensionName);
			if (m_DebugUtilsEnabled)
				requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);
			else
				CDL_CORE_WARN(LogChannel::Render, "VK_EXT_debug_utils is not available, so Vulkan objects won't be named");
		}

		// Synchronization validation is off by default in the layer, and is set through the layer's own VK_EXT_layer_settings
		vk::Bool32 enableSyncValidation = vk::True;
		vk::LayerSettingEXT syncValidationSetting{
			.pLayerName = kKhronosValidationLayer,
			.pSettingName = "validate_sync",
			.type = vk::LayerSettingTypeEXT::eBool32,
			.valueCount = 1,
			.pValues = &enableSyncValidation
		};
		vk::LayerSettingsCreateInfoEXT layerSettings{ .settingCount = 1, .pSettings = &syncValidationSetting };

		bool useLayerSettings = false;
		if (m_Specification.ValidationSpec.EnableValidation && m_Specification.ValidationSpec.EnableSynchronizationValidation)
		{
			auto layerExtensions = m_Context.enumerateInstanceExtensionProperties(std::string(kKhronosValidationLayer));
			useLayerSettings = HasExtension(layerExtensions, vk::EXTLayerSettingsExtensionName);
			if (useLayerSettings)
				requiredExtensions.push_back(vk::EXTLayerSettingsExtensionName);
			else
				CDL_CORE_WARN(LogChannel::Render, "The validation layer has no VK_EXT_layer_settings, so synchronization validation is off");
		}

		vk::InstanceCreateInfo createInfo{
			.pNext = useLayerSettings ? &layerSettings : nullptr,
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