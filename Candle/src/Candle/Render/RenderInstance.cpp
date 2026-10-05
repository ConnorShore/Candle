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

	}

	RenderInstance::RenderInstance(RenderInstanceSpecification spec)
		: m_Specification(spec)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "RenderInstance must be created on the main thread!");
		CreateInstance();
	}


	void RenderInstance::CreateInstance()
	{
		vk::ApplicationInfo appInfo{ .pApplicationName = m_Specification.ApplicationName,
											.applicationVersion = m_Specification.ApplicationVersion,
											.pEngineName = "Candle Engine",
											.engineVersion = VK_MAKE_VERSION(CANDLE_VERSION_MAJOR, CANDLE_VERSION_MINOR, CANDLE_VERSION_PATCH),
											.apiVersion = vk::ApiVersion14 };


		// Get the required validation layers
		int layerCount = m_Specification.EnableValidation ? static_cast<int>(validationLayers.size()) : 0;
		std::vector<char const*> requiredLayers;
		requiredLayers.reserve(layerCount);
		if (m_Specification.EnableValidation)
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
		auto requiredExtensions = Platform::GetVulkanRequiredInstanceExtensions(extensionCount);
		auto extensionProperties = m_Context.enumerateInstanceExtensionProperties();

		// Debug logging for now
		for (const auto& extension : extensionProperties) {
			CDL_CORE_TRACE(LogChannel::Render, "Available Vulkan Extension: {}", extension.extensionName);
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
			.enabledExtensionCount = extensionCount,
			.ppEnabledExtensionNames = requiredExtensions
		};

		// Create the Vulkan instance using the RAII wrapper. The instance will be automatically destroyed when it goes out of scope.
		m_Instance = vk::raii::Instance(m_Context, createInfo);
	}

}