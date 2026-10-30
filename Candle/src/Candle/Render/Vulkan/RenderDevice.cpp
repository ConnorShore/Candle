#include "cdlpch.h"
#include "RenderDevice.h"

#include "Candle/Render/Vulkan/RenderInstance.h"
#include "Candle/Render/RenderSpecification.h"
#include "Candle/Render/Vulkan/VulkanUtils.h"

#include "Candle/Core/Profiler.h"
#include "Candle/Core/Threading/ScopedLock.h"
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

		// Only extensions that aren't core in 1.3 and that Candle can't run without; a device missing one is skipped
		constexpr std::array<const char*, 1> kRequiredDeviceExtensions = {
			vk::KHRSwapchainExtensionName
		};

		using FeatureChain = vk::StructureChain<vk::PhysicalDeviceFeatures2,
			vk::PhysicalDeviceVulkan11Features,
			vk::PhysicalDeviceVulkan12Features,
			vk::PhysicalDeviceVulkan13Features>;
		using FeatureField = vk::Bool32& (*)(FeatureChain&);

		// The one list of required features: device selection checks each, device creation enables each
#define CDL_REQUIRED_FEATURE(Struct, Member) [](FeatureChain& chain) -> vk::Bool32& { return chain.get<Struct>().Member; }
		constexpr FeatureField kRequiredFeatures[] = {
			CDL_REQUIRED_FEATURE(vk::PhysicalDeviceFeatures2, features.samplerAnisotropy),
			CDL_REQUIRED_FEATURE(vk::PhysicalDeviceVulkan11Features, shaderDrawParameters),	// Slang lowers SV_VertexID to VertexIndex - BaseVertex
			CDL_REQUIRED_FEATURE(vk::PhysicalDeviceVulkan12Features, scalarBlockLayout),	// slangc -fvk-use-c-layout emits offsets and strides std430 rejects
			CDL_REQUIRED_FEATURE(vk::PhysicalDeviceVulkan12Features, timelineSemaphore),
			CDL_REQUIRED_FEATURE(vk::PhysicalDeviceVulkan12Features, bufferDeviceAddress),	// For VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT
			CDL_REQUIRED_FEATURE(vk::PhysicalDeviceVulkan12Features, descriptorBindingPartiallyBound),
			CDL_REQUIRED_FEATURE(vk::PhysicalDeviceVulkan12Features, descriptorBindingUpdateUnusedWhilePending),
			CDL_REQUIRED_FEATURE(vk::PhysicalDeviceVulkan12Features, descriptorBindingSampledImageUpdateAfterBind),
			CDL_REQUIRED_FEATURE(vk::PhysicalDeviceVulkan12Features, runtimeDescriptorArray),
			CDL_REQUIRED_FEATURE(vk::PhysicalDeviceVulkan13Features, dynamicRendering),
			CDL_REQUIRED_FEATURE(vk::PhysicalDeviceVulkan13Features, synchronization2),
		};
#undef CDL_REQUIRED_FEATURE

		// Queries exactly the structs in the chain, so the queried and enabled lists can't name different ones
		template <typename... Structs>
		void QueryFeatures(vk::raii::PhysicalDevice const& physicalDevice, vk::StructureChain<Structs...>& chain)
		{
			chain = physicalDevice.getFeatures2<Structs...>();
		}

		bool HasExtension(std::span<const vk::ExtensionProperties> available, const char* name)
		{
			return std::ranges::any_of(available, [name](vk::ExtensionProperties const& extension) { return strcmp(extension.extensionName, name) == 0; });
		}

		struct PhysicalDeviceSelection
		{
			vk::raii::PhysicalDevice PhysicalDevice;
			QueueFamilyIndices QueueFamilies;
		};

		// Returns the queue families to use, or why the device can't be used
		std::expected<QueueFamilyIndices, std::string> CheckPhysicalDevice(RenderInstance& instance, const RenderSpecification& renderSpec, vk::raii::PhysicalDevice const& physicalDevice)
		{
			// Checked first: querying the 1.3 feature structs below is invalid on an older device
			if (physicalDevice.getProperties().apiVersion < ToVulkanApiVersion(renderSpec.VulkanVersion))
				return std::unexpected("Vulkan version is too old");

			auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
			bool supportsAllRequiredExtensions = std::ranges::all_of(kRequiredDeviceExtensions,
				[&availableDeviceExtensions](const char* extension) { return HasExtension(availableDeviceExtensions, extension); });
			if (!supportsAllRequiredExtensions)
				return std::unexpected("missing a required extension");

			FeatureChain supportedFeatures;
			QueryFeatures(physicalDevice, supportedFeatures);
			bool supportsRequiredFeatures = std::ranges::all_of(kRequiredFeatures,
				[&supportedFeatures](FeatureField field) { return field(supportedFeatures) == vk::True; });
			if (!supportsRequiredFeatures)
				return std::unexpected("missing a required feature");

			// Find queue families that support graphics + present, and a dedicated transfer queue if available
			const std::vector<vk::QueueFamilyProperties> families = physicalDevice.getQueueFamilyProperties();
			std::vector<vk::Bool32> presentSupport(families.size());
			for (uint32_t i = 0; i < families.size(); ++i)
				presentSupport[i] = Platform::GetVulkanPresentationSupport(instance, physicalDevice, i);

			std::optional<QueueFamilyIndices> queueFamilies = FindQueueFamilies(families, presentSupport);
			if (!queueFamilies)
				return std::unexpected("no graphics queue family that can present");

			return *queueFamilies;
		}

		bool IsPreferredDeviceType(RenderDevicePreference preference, vk::PhysicalDeviceType type)
		{
			switch (preference)
			{
			case RenderDevicePreference::DiscreteGPU:	return type == vk::PhysicalDeviceType::eDiscreteGpu;
			case RenderDevicePreference::IntegratedGPU:	return type == vk::PhysicalDeviceType::eIntegratedGpu;
			default:									return true;
			}
		}

		// The preference only ranks suitable devices, so a machine without the preferred type still gets one
		std::expected<PhysicalDeviceSelection, std::string> SelectPhysicalDevice(RenderInstance& instance, const RenderSpecification& renderSpec)
		{
			std::optional<PhysicalDeviceSelection> fallback;
			std::string rejectedDevices;

			for (vk::raii::PhysicalDevice& physicalDevice : instance.GetVulkanInstance().enumeratePhysicalDevices())
			{
				std::string deviceName = physicalDevice.getProperties().deviceName;
				auto queueFamilies = CheckPhysicalDevice(instance, renderSpec, physicalDevice);
				if (!queueFamilies)
				{
					rejectedDevices += std::format("\n  {}: {}", deviceName, queueFamilies.error());
					continue;
				}

				if (IsPreferredDeviceType(renderSpec.DevicePreference, physicalDevice.getProperties().deviceType))
				{
					CDL_CORE_INFO(LogChannel::Render, "Selected Render Device: {0}", deviceName);
					return PhysicalDeviceSelection{ std::move(physicalDevice), *queueFamilies };
				}

				if (!fallback)
					fallback = PhysicalDeviceSelection{ std::move(physicalDevice), *queueFamilies };
			}

			if (fallback)
			{
				std::string deviceName = fallback->PhysicalDevice.getProperties().deviceName;
				CDL_CORE_WARN(LogChannel::Render, "No device of the preferred type found. Selected Render Device: {0}", deviceName);
				return std::move(*fallback);
			}

			return std::unexpected("Failed to find a suitable GPU" + rejectedDevices);
		}
	}

	RenderDevice::RenderDevice(RenderInstance& instance, const RenderSpecification& renderSpec)
	{
		auto selection = SelectPhysicalDevice(instance, renderSpec);
		if (!selection)
			CDL_CORE_PANIC(LogChannel::Render, "{}", selection.error());

		m_PhysicalDevice = std::move(selection->PhysicalDevice);
		m_QueueFamilies = selection->QueueFamilies;
		m_DebugUtilsEnabled = instance.IsDebugUtilsEnabled();
		CreateLogicalDeviceAndQueues();
	}

	void RenderDevice::SetDebugName(vk::ObjectType type, uint64_t handle, const std::string& name) const
	{
		if (!m_DebugUtilsEnabled || handle == 0)
			return;

		m_LogicalDevice.setDebugUtilsObjectNameEXT({ .objectType = type, .objectHandle = handle, .pObjectName = name.c_str() });
	}

	void RenderDevice::BeginDebugLabel(vk::raii::CommandBuffer& commandBuffer, const char* label) const
	{
		if (m_DebugUtilsEnabled)
			commandBuffer.beginDebugUtilsLabelEXT({ .pLabelName = label });
	}

	void RenderDevice::EndDebugLabel(vk::raii::CommandBuffer& commandBuffer) const
	{
		if (m_DebugUtilsEnabled)
			commandBuffer.endDebugUtilsLabelEXT();
	}

	GPUDebugScope::GPUDebugScope(const RenderDevice& renderDevice, vk::raii::CommandBuffer& commandBuffer, const char* label)
		: m_RenderDevice(renderDevice), m_CommandBuffer(commandBuffer)
	{
		m_RenderDevice.BeginDebugLabel(m_CommandBuffer, label);
	}

	GPUDebugScope::~GPUDebugScope()
	{
		m_RenderDevice.EndDebugLabel(m_CommandBuffer);
	}

	uint64_t RenderDevice::Submit(QueueType queue, const SubmitDesc& desc)
	{
		CDL_PROFILE_SCOPE("RenderDevice::Submit");

		CDL_CORE_ASSERT(desc.Signals.size() <= SubmitDesc::kMaxSignals, "Too many signal semaphores in one submit");

		QueueState& state = *m_Queues[std::to_underlying(queue)];

		std::array<vk::SemaphoreSubmitInfo, SubmitDesc::kMaxSignals + 1> signals{};
		std::ranges::copy(desc.Signals, signals.begin());

		// Timeline waiters reset command pools and free memory, so every stage of the submission must be finished
		vk::SemaphoreSubmitInfo& timelineSignal = signals[desc.Signals.size()];
		timelineSignal.semaphore = *state.Timeline;
		timelineSignal.stageMask = vk::PipelineStageFlagBits2::eAllCommands;

		vk::SubmitInfo2 submitInfo{
			.waitSemaphoreInfoCount = static_cast<uint32_t>(desc.Waits.size()),
			.pWaitSemaphoreInfos = desc.Waits.data(),
			.commandBufferInfoCount = static_cast<uint32_t>(desc.CommandBuffers.size()),
			.pCommandBufferInfos = desc.CommandBuffers.data(),
			.signalSemaphoreInfoCount = static_cast<uint32_t>(desc.Signals.size() + 1),
			.pSignalSemaphoreInfos = signals.data() };

		ScopedLock<AdaptiveSpinLock> lock(state.SubmitLock);
		timelineSignal.value = ++state.LastSubmitted;
		state.Queue.submit2(submitInfo);
		return timelineSignal.value;
	}

	vk::Result RenderDevice::Present(const vk::PresentInfoKHR& presentInfo)
	{
		QueueState& state = *m_Queues[std::to_underlying(QueueType::Graphics)];
		ScopedLock<AdaptiveSpinLock> lock(state.SubmitLock);
		return state.Queue.presentKHR(presentInfo);
	}

	void RenderDevice::WaitFor(QueueType queue, uint64_t value) const
	{
		CDL_PROFILE_WAIT("RenderDevice::WaitFor");

		const QueueState& state = *m_Queues[std::to_underlying(queue)];
		vk::SemaphoreWaitInfo waitInfo{ .semaphoreCount = 1, .pSemaphores = &*state.Timeline, .pValues = &value };

		// With no timeout the only result that doesn't throw is eSuccess
		[[maybe_unused]] vk::Result result = m_LogicalDevice.waitSemaphores(waitInfo, UINT64_MAX);
		CDL_CORE_ASSERT(result == vk::Result::eSuccess, "Untimed timeline wait returned early");
	}

	uint64_t RenderDevice::GetCompletedTimelineValue(QueueType queue) const
	{
		return m_Queues[std::to_underlying(queue)]->Timeline.getCounterValue();
	}

	void RenderDevice::WaitIdle()
	{
		CDL_PROFILE_WAIT("RenderDevice::WaitIdle");

		// Locks the storage, not m_Queues, so an aliased queue's lock isn't taken twice
		ScopedLock<AdaptiveSpinLock> graphicsLock(m_QueueStates[std::to_underlying(QueueType::Graphics)].SubmitLock);
		ScopedLock<AdaptiveSpinLock> transferLock(m_QueueStates[std::to_underlying(QueueType::Transfer)].SubmitLock);
		m_LogicalDevice.waitIdle();
	}

	void RenderDevice::CreateLogicalDeviceAndQueues()
	{
		if (m_QueueFamilies.Transfer == m_QueueFamilies.Graphics)
			CDL_CORE_WARN(LogChannel::Render, "No dedicated transfer queue found. Using graphics queue for transfer operations.");

		// A feature must be enabled to be used, even one 1.3 guarantees every device supports
		FeatureChain featureChain;
		for (FeatureField field : kRequiredFeatures)
			field(featureChain) = vk::True;

		// Without memory budget VMA estimates the budget from heap sizes, so it's enabled only when present
		std::vector<const char*> enabledExtensions(kRequiredDeviceExtensions.begin(), kRequiredDeviceExtensions.end());
		m_HasMemoryBudget = HasExtension(m_PhysicalDevice.enumerateDeviceExtensionProperties(), vk::EXTMemoryBudgetExtensionName);
		if (m_HasMemoryBudget)
			enabledExtensions.push_back(vk::EXTMemoryBudgetExtensionName);

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
												   .enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
												   .ppEnabledExtensionNames = enabledExtensions.data() };

		m_LogicalDevice = vk::raii::Device(m_PhysicalDevice, deviceCreateInfo);
		SetDebugName(*m_LogicalDevice, "Candle Device");

		vk::SemaphoreTypeCreateInfo timelineType{ .semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = 0 };
		vk::SemaphoreCreateInfo timelineCreateInfo{ .pNext = &timelineType };

		// Queue index 0 within each family, the only queue created in it
		auto createQueue = [&](QueueType queue, uint32_t family, const std::string& name)
		{
			QueueState& state = m_QueueStates[std::to_underlying(queue)];
			state.Queue = vk::raii::Queue(m_LogicalDevice, family, 0);
			state.Timeline = vk::raii::Semaphore(m_LogicalDevice, timelineCreateInfo);
			SetDebugName(*state.Queue, name + " Queue");
			SetDebugName(*state.Timeline, name + " Timeline");
			return &state;
		};

		constexpr size_t graphicsSlot = std::to_underlying(QueueType::Graphics);
		constexpr size_t transferSlot = std::to_underlying(QueueType::Transfer);
		m_Queues[graphicsSlot] = createQueue(QueueType::Graphics, m_QueueFamilies.Graphics, "Graphics");
		if (m_QueueFamilies.Transfer == m_QueueFamilies.Graphics)
			m_Queues[transferSlot] = m_Queues[graphicsSlot];
		else
			m_Queues[transferSlot] = createQueue(QueueType::Transfer, m_QueueFamilies.Transfer, "Transfer");
	}

}