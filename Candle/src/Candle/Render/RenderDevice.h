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
		uint32_t Graphics;   // Graphics + compute + transfer + present; still verified per surface when a swapchain is created
		uint32_t Transfer;   // Transfer only queue (if available, otherwise graphics queue)
	};

	// Returns the queue family indices for graphics and transfer queues, or std::nullopt if the device is unsuitable
	std::optional<QueueFamilyIndices> FindQueueFamilies(std::span<const vk::QueueFamilyProperties> families, std::span<const vk::Bool32> presentSupport);

	struct SubmitDesc
	{
		static constexpr size_t kMaxSignals = 3;	// One more slot is reserved for the queue's timeline

		std::span<const vk::CommandBufferSubmitInfo> CommandBuffers;
		std::span<const vk::SemaphoreSubmitInfo> Waits;
		std::span<const vk::SemaphoreSubmitInfo> Signals;	// Binary semaphores only; Submit appends the queue's timeline
	};

	// Holds the vulkan physical & logical device, queues and their timelines, command and resource pools, deletion queues etc
	// Will own buffer pools to be allocated from, but the buffers themselves will be owned by the Mesh asset
	// this will own command pools as well, but CommandLists will be short lived and handed out each frame
	// Knows nothing about frames: frame slots and their retire values belong to RenderManager
	// Resource creation, submission and timeline waits can happen from any thread
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
		//   few submits, not one per draw. The timeline value is reserved under the same lock: signals execute in
		//   submission order and each must exceed the semaphore's current value, so a value reserved outside it could
		//   reach the queue after a larger one.
		// Returns the value signalled; once the GPU reaches it, this and every earlier submission to the queue is done.
		uint64_t Submit(QueueType queue, const SubmitDesc& desc);
		vk::Result Present(const vk::PresentInfoKHR& presentInfo);	// Graphics queue, under its submit lock

		void WaitFor(QueueType queue, uint64_t value) const;	// Blocks until the queue's timeline reaches value; 0 returns at once
		void WaitIdle(); // Shutdown only, never in the frame loop. vkDeviceWaitIdle synchronizes on every queue, so it takes every submit lock.

		inline vk::raii::Device& GetDevice() { return m_LogicalDevice; }
		inline vk::raii::PhysicalDevice& GetPhysicalDevice() { return m_PhysicalDevice; }

		inline uint32_t GetQueueFamily(QueueType queue) const { return queue == QueueType::Graphics ? m_QueueFamilies.Graphics : m_QueueFamilies.Transfer; }

	private:
		// One per distinct VkQueue; roles aliasing one VkQueue point at the same state, sharing its lock and timeline.
		struct QueueState
		{
			vk::raii::Queue Queue{ nullptr };
			vk::raii::Semaphore Timeline{ nullptr };
			uint64_t LastSubmitted{ 0 };	// Guarded by SubmitLock
			AdaptiveSpinLock SubmitLock;
		};

		void CreateLogicalDeviceAndQueues();

	private:
		vk::raii::PhysicalDevice m_PhysicalDevice{ nullptr };
		QueueFamilyIndices m_QueueFamilies{};
		vk::raii::Device m_LogicalDevice{ nullptr };

		std::array<QueueState, std::to_underlying(QueueType::Count)> m_QueueStates;	// The Transfer entry goes unused while Transfer aliases Graphics
		std::array<QueueState*, std::to_underlying(QueueType::Count)> m_Queues{};	// Indexed by QueueType, every slot filled
	};

}