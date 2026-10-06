#include "cdlpch.h"
#include "RenderDevice.h"

#include "RenderInstance.h"
#include "RenderSpecification.h"
#include "VulkanUtils.h"

namespace Candle {

	// Graphics needs graphics + compute (either implies transfer)
	// Transfer prefers a dedicated DMA family.
	std::optional<QueueFamilyIndices> FindQueueFamilies(std::span<const vk::QueueFamilyProperties> families)
	{
		constexpr vk::QueueFlags graphicsCompute = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
		// Sparse binding and protected don't stop a family being a copy engine; video or compute bits do.
		constexpr vk::QueueFlags dmaFlags = vk::QueueFlagBits::eTransfer | vk::QueueFlagBits::eSparseBinding | vk::QueueFlagBits::eProtected;

		std::optional<uint32_t> graphics, transfer;
		for (uint32_t i = 0; i < families.size(); ++i)
		{
			vk::QueueFlags flags = families[i].queueFlags;
			if (!graphics && (flags & graphicsCompute) == graphicsCompute)
				graphics = i;
			if (!transfer && (flags & vk::QueueFlagBits::eTransfer) && !(flags & ~dmaFlags))
				transfer = i;
		}

		if (!graphics)
			return std::nullopt;

		return QueueFamilyIndices{ .Graphics = *graphics, .Transfer = transfer.value_or(*graphics) };
	}

	namespace {

		std::vector<const char*> requiredDeviceExtension = {
			vk::KHRSwapchainExtensionName
		};

		struct PhysicalDeviceSelection
		{
			vk::raii::PhysicalDevice PhysicalDevice;
			QueueFamilyIndices QueueFamilies;
		};

		std::optional<QueueFamilyIndices> IsPhysicalDeviceSuitable(RenderSpecification renderSpec, vk::raii::PhysicalDevice const& physicalDevice)
		{
			// Check if the physicalDevice supports the Vulkan API requested
			bool supportsVulkan = physicalDevice.getProperties().apiVersion >= ToVulkanApiVersion(renderSpec.VulkanVersion);

			// Check if the physicalDevice is of the preferred type (discrete or integrated GPU)
			// TODO: Fix: If user sets DevicePreference to DiscreteGPU, but the only available GPU is an IntegratedGPU, we need to fall back to it or throw an error
			bool isPreferredDeviceType = false;
			if (renderSpec.DevicePreference == RenderDevicePreference::DiscreteGPU)
				isPreferredDeviceType = physicalDevice.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
			else if (renderSpec.DevicePreference == RenderDevicePreference::IntegratedGPU)
				isPreferredDeviceType = physicalDevice.getProperties().deviceType == vk::PhysicalDeviceType::eIntegratedGpu;
			else
				isPreferredDeviceType = true;

			// Check if all required physicalDevice extensions are available
			auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
			bool supportsAllRequiredExtensions =
				std::ranges::all_of(requiredDeviceExtension,
					[&availableDeviceExtensions](auto const& requiredDeviceExtension)
					{
						return std::ranges::any_of(availableDeviceExtensions,
							[requiredDeviceExtension](auto const& availableDeviceExtension)
							{ return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
					});

			// Check if the physicalDevice supports the required features (shader draw parameters, dynamic rendering and extended dynamic state)
			auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
				vk::PhysicalDeviceVulkan11Features,
				vk::PhysicalDeviceVulkan13Features,
				vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
			bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
				features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
				features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
				features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
				features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

			// Suitable devices also report the queue families to use, so selection and creation can't disagree
			if (!supportsVulkan || !supportsAllRequiredExtensions || !supportsRequiredFeatures)
				return std::nullopt;
			return FindQueueFamilies(physicalDevice.getQueueFamilyProperties());
		}

		PhysicalDeviceSelection SelectPhysicalDevice(RenderInstance& instance, const RenderSpecification& renderSpec)
		{
			for (vk::raii::PhysicalDevice& physicalDevice : instance.GetVulkanInstance().enumeratePhysicalDevices())
			{
				if (auto queueFamilies = IsPhysicalDeviceSuitable(renderSpec, physicalDevice))
				{
					std::string deviceName = physicalDevice.getProperties().deviceName;
					CDL_CORE_INFO(LogChannel::Render, "Selected Render Device: {0}", deviceName);
					return { std::move(physicalDevice), *queueFamilies };
				}
			}

			throw std::runtime_error("Failed to find a suitable GPU!");
		}
	}

	RenderDevice::RenderDevice(RenderInstance& instance, const RenderSpecification& renderSpec)
	{
		PhysicalDeviceSelection selection = SelectPhysicalDevice(instance, renderSpec);
		m_PhysicalDevice = std::move(selection.PhysicalDevice);
		m_QueueFamilies = selection.QueueFamilies;
		CreateLogicalDeviceAndQueues();
	}

	void RenderDevice::CreateLogicalDeviceAndQueues()
	{
		if (m_QueueFamilies.Transfer == m_QueueFamilies.Graphics)
			CDL_CORE_WARN(LogChannel::Render, "No dedicated transfer queue found. Using graphics queue for transfer operations.");

		// query for Vulkan features and extensions, and enable the required ones
		vk::StructureChain<vk::PhysicalDeviceFeatures2,
			vk::PhysicalDeviceVulkan13Features,
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
			vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>
			featureChain = {
				{.features = {.samplerAnisotropy = true}},                   // vk::PhysicalDeviceFeatures2
				{.synchronization2 = true, .dynamicRendering = true},        // vk::PhysicalDeviceVulkan13Features
				{.extendedDynamicState = true},                              // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
				{.timelineSemaphore = true}                                  // vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR
		};

		const float queuePriority = 1.0f;

		uint32_t numQueueFamilies = m_QueueFamilies.Graphics == m_QueueFamilies.Transfer ? 1 : 2;
		std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
		queueCreateInfos.reserve(numQueueFamilies);
		for (uint32_t i = 0; i < numQueueFamilies; ++i)
		{
			uint32_t queueIndex = (i == 0) ? m_QueueFamilies.Graphics : m_QueueFamilies.Transfer;
			queueCreateInfos.emplace_back(vk::DeviceQueueCreateInfo{ .queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority });
		}

		vk::DeviceCreateInfo deviceCreateInfo{ .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
												   .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
												   .pQueueCreateInfos = queueCreateInfos.data(),
												   .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
												   .ppEnabledExtensionNames = requiredDeviceExtension.data() };

		m_LogicalDevice = vk::raii::Device(m_PhysicalDevice, deviceCreateInfo);
		m_Queues.insert_or_assign(QueueType::Graphics, vk::raii::Queue(m_LogicalDevice, m_QueueFamilies.Graphics, 0));
		if (m_QueueFamilies.Transfer != m_QueueFamilies.Graphics)
			m_Queues.insert_or_assign(QueueType::Transfer, vk::raii::Queue(m_LogicalDevice, m_QueueFamilies.Transfer, 0));	// Index within the family, which has one queue
	}

}