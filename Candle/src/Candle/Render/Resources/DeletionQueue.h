#pragma once

#include "Candle/Render/Vulkan/QueueType.h"
#include "Candle/Render/Resources/GPUResource.h"

#include <array>
#include <cstdint>
#include <deque>
#include <utility>

namespace Candle {

	// Holds GPU objects until the queue they were last used on reaches their retire value.
	// Threading: not synchronized; the one thread that owns it both enqueues and drains.
	class DeletionQueue
	{
	public:
		DeletionQueue() = default;

		DeletionQueue(const DeletionQueue&) = delete;
		DeletionQueue& operator=(const DeletionQueue&) = delete;

		void Enqueue(QueueType queue, uint64_t retireValue, GPUResource&& resource);
		void Drain(QueueType queue, uint64_t completedValue);	// Destroys everything whose retire value the queue has reached
		void DrainAll();	// Only once the GPU is idle

	private:
		struct Entry
		{
			GPUResource Resource;
			uint64_t RetireValue;
		};

		std::array<std::deque<Entry>, std::to_underlying(QueueType::Count)> m_Entries;
	};

}
