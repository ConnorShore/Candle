#pragma once

#include "Candle/Core/Core.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace Candle {

	// A lock-free, single-consumer, multiple-producer ring buffer. The consumer is expected to be a
	// single thread that drains the buffer in a tight loop, and the producers are expected to be
	// multiple threads that push to the buffer in a tight loop.
	template<typename T>
	class MPSCRingBuffer
	{
		CDL_STATIC_ASSERT(std::is_trivially_copyable_v<T>,
			"Slots are published by raw copy, so T must be trivially copyable.");
		CDL_STATIC_ASSERT(std::is_default_constructible_v<T>,
			"The slot array is default-constructed up front.");

	public:
		// Capacity must be a power of two so the wrap is a mask rather than a 64-bit divide.
		explicit MPSCRingBuffer(size_t capacity)
			: m_Capacity(capacity), m_Mask(capacity - 1)
		{
			CDL_CORE_ASSERT(capacity > 0 && (capacity & (capacity - 1)) == 0,
				"MPSCRingBuffer capacity must be a power of two");
			m_Buffer = new Slot[capacity];
		}

		~MPSCRingBuffer()
		{
			delete[] m_Buffer;
		}

		MPSCRingBuffer(const MPSCRingBuffer&) = delete;
		MPSCRingBuffer& operator=(const MPSCRingBuffer&) = delete;

		bool TryPush(const T& item)
		{
			size_t currentHead = m_Head.load(std::memory_order_relaxed);

			while (true)
			{
				// Head and tail are free-running and never wrapped, so this difference is an exact
				// occupancy. A stale tail only ever over-reports fullness, which is the safe direction.
				size_t currentTail = m_Tail.load(std::memory_order_acquire);
				if (currentHead - currentTail >= m_Capacity)
				{
					// Buffer is full, drop the item and increment the dropped count
					m_Dropped.fetch_add(1, std::memory_order_relaxed);
					return false;
				}

				if (m_Head.compare_exchange_weak(currentHead, currentHead + 1, std::memory_order_relaxed, std::memory_order_relaxed))
					break; // Successfully reserved a slot

				// If compare_exchange_weak fails, currentHead is updated to the current value of m_Head, so we can retry
			}

			// We've successfully reserved a slot at currentHead. Now we can write the item.
			size_t index = currentHead & m_Mask;

			// Ensure that the slot is empty before writing to it
			CDL_CORE_ASSERT(m_Buffer[index].State.load(std::memory_order_acquire) == SlotState::Empty,
				"Producer claimed a slot the consumer has not released");

			m_Buffer[index].Value = item;
			m_Buffer[index].State.store(SlotState::Written, std::memory_order_release);
			return true;
		}

		std::optional<T> Pop()
		{
			size_t currentTail = m_Tail.load(std::memory_order_relaxed);
			size_t index = currentTail & m_Mask;

			// Ensure that the slot is written before trying to read it
			if (m_Buffer[index].State.load(std::memory_order_acquire) != SlotState::Written)
				return std::nullopt; // Buffer is empty

			// Read the item and mark the slot as empty
			T item = std::move(m_Buffer[index].Value);
			m_Buffer[index].State.store(SlotState::Empty, std::memory_order_release);
			
			// Move the tail forward (safe since it's single-consumer)
			m_Tail.store(currentTail + 1, std::memory_order_release);
			return item;
		}

		size_t PopBatch(T* out, size_t maxCount)
		{
			size_t currentTail = m_Tail.load(std::memory_order_relaxed);
			size_t count = 0;

			while (count < maxCount)
			{
				Slot& slot = m_Buffer[currentTail & m_Mask];
				if (slot.State.load(std::memory_order_acquire) != SlotState::Written)
					break;

				out[count++] = slot.Value;

				// Freeing ahead of the tail store is safe for the same reason as Pop: producers still
				// see the old, smaller tail, so they cannot reach these slots any sooner.
				slot.State.store(SlotState::Empty, std::memory_order_release);
				++currentTail;
			}

			if (count > 0)
				m_Tail.store(currentTail, std::memory_order_release);

			return count;
		}

		// Any thread. Records rejected by TryPush since construction.
		uint64_t DroppedCount() const { return m_Dropped.load(std::memory_order_relaxed); }
		size_t Capacity() const { return m_Capacity; }

	private:
		enum class SlotState { Empty, Written };
		struct Slot
		{
			T Value;
			std::atomic<SlotState> State{ SlotState::Empty };
		};

		// Immutable after construction, so this line is shared read-only and never invalidated.
		Slot* m_Buffer = nullptr;
		const size_t m_Capacity;
		const size_t m_Mask;

		// Producers own this line: they CAS the head and, on the rare full path, bump the counter.
		alignas(64) std::atomic<size_t> m_Head{ 0 };
		std::atomic<uint64_t> m_Dropped{ 0 };

		// Consumer writes, producers read. alignas forces the class size to a multiple of 64, so the
		// trailing padding already keeps this line clear of whatever is allocated next.
		alignas(64) std::atomic<size_t> m_Tail{ 0 };
	};
}