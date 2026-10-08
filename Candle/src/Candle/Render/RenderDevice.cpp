#include "cdlpch.h"
#include "RenderDevice.h"

#include "RenderInstance.h"
#include "RenderSpecification.h"
#include "VulkanUtils.h"

#include "Candle/Platform/Platform.h"

namespace Candle {

	// Graphics needs graphics + compute (either implies transfer) + present
	// Transfer prefers a dedicated DMA family.
	std::optional<QueueFamilyIndices> FindQueueFamilies(std::span<const vk::QueueFamilyProperties> families, std::span<const vk::Bool32> presentSupport)
	{
		CDL_CORE_ASSERT(presentSupport.size() == families.size(), "Need one present-support entry per queue family");

		constexpr vk::QueueFlags graphicsCompute = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
		// Sparse binding and protected don't stop a family being a copy engine; video or compute bits do.
		constexpr vk::QueueFlags dmaFlags = vk::QueueFlagBits::eTransfer | vk::QueueFlagBits::eSparseBinding | vk::QueueFlagBits::eProtected;

		std::optional<uint32_t> graphics, transfer;
		for (uint32_t i = 0; i < families.size(); ++i)
		{
			vk::QueueFlags flags = families[i].queueFlags;
			if (!graphics && (flags & graphicsCompute) == graphicsCompute && presentSupport[i])
				graphics = i;
			if (!transfer && (flags & vk::QueueFlagBits::eTransfer) && !(flags & ~dmaFlags))
				transfer = i;
		}

		if (!graphics)
			return std::nullopt;

		return QueueFamilyIndices{ .Graphics = *graphics, .Transfer = transfer.value_or(*graphics) };
	}

	namespace {

		constexpr std::array<const char*, 1> kRequiredDeviceExtensions = {
			vk::KHRSwapchainExtensionName
		};

		struct PhysicalDeviceSelection
		{
			vk::raii::PhysicalDevice PhysicalDevice;
			QueueFamilyIndices QueueFamilies;
		};

		std::optional<QueueFamilyIndices> IsPhysicalDeviceSuitable(RenderInstance& instance, RenderSpecification renderSpec, vk::raii::PhysicalDevice const& physicalDevice)
		{
			bool supportsVulkan = physicalDevice.getProperties().apiVersion >= ToVulkanApiVersion(renderSpec.VulkanVersion);

			// TODO: Fix: If user sets DevicePreference to DiscreteGPU, but the only available GPU is an IntegratedGPU, we need to fall back to it or throw an error
			bool isPreferredDeviceType = false;
			if (renderSpec.DevicePreference == RenderDevicePreference::DiscreteGPU)
				isPreferredDeviceType = physicalDevice.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
			else if (renderSpec.DevicePreference == RenderDevicePreference::IntegratedGPU)
				isPreferredDeviceType = physicalDevice.getProperties().deviceType == vk::PhysicalDeviceType::eIntegratedGpu;
			else
				isPreferredDeviceType = true;

			auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
			bool supportsAllRequiredExtensions =
				std::ranges::all_of(kRequiredDeviceExtensions,
					[&availableDeviceExtensions](auto const& requiredDeviceExtension)
					{
						return std::ranges::any_of(availableDeviceExtensions,
							[requiredDeviceExtension](auto const& availableDeviceExtension)
							{ return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
					});

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
			if (!supportsVulkan || !supportsAllRequiredExtensions || !supportsRequiredFeatures || !isPreferredDeviceType)
				return std::nullopt;

			// Find queue families that support graphics + present, and a dedicated transfer queue if available
			const std::vector<vk::QueueFamilyProperties> families = physicalDevice.getQueueFamilyProperties();
			std::vector<vk::Bool32> presentSupport(families.size());
			for (uint32_t i = 0; i < families.size(); ++i)
				presentSupport[i] = Platform::GetVulkanPresentationSupport(instance, physicalDevice, i);
			return FindQueueFamilies(families, presentSupport);
		}

		PhysicalDeviceSelection SelectPhysicalDevice(RenderInstance& instance, const RenderSpecification& renderSpec)
		{
			for (vk::raii::PhysicalDevice& physicalDevice : instance.GetVulkanInstance().enumeratePhysicalDevices())
			{
				if (auto queueFamilies = IsPhysicalDeviceSuitable(instance, renderSpec, physicalDevice))
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
			vk::PhysicalDeviceVulkan11Features,
			vk::PhysicalDeviceVulkan13Features,
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
			vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>
			featureChain = {
				{.features = {.samplerAnisotropy = true}},                   // vk::PhysicalDeviceFeatures2
				{.shaderDrawParameters = true },							 // vk::PhysicalDeviceVulkan11Features
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
												   .enabledExtensionCount = static_cast<uint32_t>(kRequiredDeviceExtensions.size()),
												   .ppEnabledExtensionNames = kRequiredDeviceExtensions.data() };

		m_LogicalDevice = vk::raii::Device(m_PhysicalDevice, deviceCreateInfo);

		// Queue index 0 within each family, the only queue created in it
		constexpr size_t graphicsSlot = std::to_underlying(QueueType::Graphics);
		constexpr size_t transferSlot = std::to_underlying(QueueType::Transfer);
		m_Queues[graphicsSlot] = { vk::raii::Queue(m_LogicalDevice, m_QueueFamilies.Graphics, 0), &m_SubmitLocks[graphicsSlot] };
		if (m_QueueFamilies.Transfer == m_QueueFamilies.Graphics)
			m_Queues[transferSlot] = m_Queues[graphicsSlot];
		else
			m_Queues[transferSlot] = { vk::raii::Queue(m_LogicalDevice, m_QueueFamilies.Transfer, 0), &m_SubmitLocks[transferSlot] };
	}

}