#include "cdlpch.h"
#include "DeletionQueue.h"

namespace Candle {

	void DeletionQueue::Enqueue(QueueType queue, uint64_t retireValue, GPUResource&& resource)
	{
		std::deque<Entry>& entries = m_Entries[std::to_underlying(queue)];
		CDL_CORE_ASSERT(entries.empty() || entries.back().RetireValue <= retireValue, "Retire values must not decrease, or Drain would stop early");
		entries.push_back({ std::move(resource), retireValue });
	}

	void DeletionQueue::Drain(QueueType queue, uint64_t completedValue)
	{
		std::deque<Entry>& entries = m_Entries[std::to_underlying(queue)];
		while (!entries.empty() && entries.front().RetireValue <= completedValue)
			entries.pop_front();
	}

	void DeletionQueue::DrainAll()
	{
		for (std::deque<Entry>& entries : m_Entries)
			entries.clear();
	}

}
