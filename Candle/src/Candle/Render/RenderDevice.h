#pragma once

#include "Candle/Core/Threading/AdaptiveSpinLock.h"

#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <optional>
#include <span>
#include <utility>

namespace Candle {

	class RenderInstance;
	struct RenderSpecification;

	enum class QueueType : uint8_t
	{
		Graphics,
		Transfer,
		Count
	};

	struct QueueFamilyIndices
	{
		uint32_t Graphics;   // Graphics + compute + transfer; also presents on Windows, verified per swapchain
		uint32_t Transfer;   // Transfer only queue (if available, otherwise graphics queue)
	};

	std::optional<QueueFamilyIndices> FindQueueFamilies(std::span<const vk::QueueFamilyProperties> families);

	// Holds the vulkan physical & logical device, queues, command and resource pools, frame timeline, deletion queues etc
	// Will own buffer pools to be allocated from, but the buffers themselves will be owned by the Mesh asset
	// this will own command pools as well, but CommandLists will be short lived and handed out each frame
	// Resource creation can happen from any thread, but submission is from render thread only
	class RenderDevice
	{
	public:
		RenderDevice(RenderInstance& instance, const RenderSpecification& renderSpec);
		~RenderDevice() = default;

		// Delete copy constructor and assignment operator
		RenderDevice(const RenderDevice&) = delete;
		RenderDevice& operator=(const RenderDevice&) = delete;

		// Threading: vkQueueSubmit2 requires external synchronization per VkQueue. Each distinct vulkan Queue gets one
		//   submit lock; roles that alias the same vulkan Queue share it. A lock is fine here because a frame makes a
		//   few submits, not one per draw.
		//void Submit(QueueType queue, std::span<const vk::SubmitInfo2> submits);

		inline vk::raii::Device& GetDevice() { return m_LogicalDevice; }
		inline uint32_t GetQueueFamily(QueueType queue) const { return queue == QueueType::Graphics ? m_QueueFamilies.Graphics : m_QueueFamilies.Transfer; }

	private:
		// A role's queue and the lock guarding submission to it; roles aliasing one VkQueue copy the handle and share the lock.
		struct QueueSlot
		{
			vk::raii::Queue Queue{ nullptr };
			AdaptiveSpinLock* SubmitLock = nullptr;
		};

		void CreateLogicalDeviceAndQueues();

	private:
		vk::raii::PhysicalDevice m_PhysicalDevice{ nullptr };
		QueueFamilyIndices m_QueueFamilies{};
		vk::raii::Device m_LogicalDevice{ nullptr };

		// One lock per distinct VkQueue; the Transfer lock goes unused while Transfer aliases Graphics.
		std::array<AdaptiveSpinLock, std::to_underlying(QueueType::Count)> m_SubmitLocks;
		std::array<QueueSlot, std::to_underlying(QueueType::Count)> m_Queues;	// Indexed by QueueType, every slot filled
	};

}