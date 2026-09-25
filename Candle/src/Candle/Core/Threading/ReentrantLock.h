#pragma once

#include "Candle/Core/Core.h"

#include <atomic>
#include <thread>

namespace Candle {

	// ReentrantLock is a lock that can be acquired multiple times by the same thread without causing a deadlock
	// unlike the standard SpinLock or AdaptiveSpinLock, which will deadlock if the same thread tries to acquire the lock again before releasing it.

	class ReentrantLock
	{
	public:
		inline void Acquire()
		{
const size_t threadId = static_cast<size_t>(Platform::GetCurrentThreadId());

			if (m_Atomic.load(std::memory_order_relaxed) != threadId)
			{
				// Spin until we can set the thread ID to the current thread
				size_t unlockVal = 0;
				while (!m_Atomic.compare_exchange_weak(unlockVal, threadId, std::memory_order_relaxed, std::memory_order_relaxed))
				{
					unlockVal = 0; // Reset unlockVal for the next iteration
					CDL_THREAD_PAUSE();
				}
			}

			// Increment the reference count for reentrant locking
			++m_RefCount;

			// Ensure that all writes before this point are visible to other threads that acquire the lock
			std::atomic_thread_fence(std::memory_order_acquire);
		}

		inline void Release()
		{
			// Use release semantics to ensure that all writes made while holding the lock are visible
			std::atomic_thread_fence(std::memory_order_release);

			std::hash<std::thread::id> hasher;
			size_t threadId = hasher(std::this_thread::get_id());
			size_t actual = m_Atomic.load(std::memory_order_relaxed);

			//assert(actual == threadId);	// TODO: Add assertion here once implemented

			if (--m_RefCount == 0)
			{
				// Reset the thread ID to indicate that the lock is now free
				m_Atomic.store(0, std::memory_order_relaxed);
			}
		}

		inline bool TryAcquire()
		{
			std::hash<std::thread::id> hasher;
			size_t threadId = hasher(std::this_thread::get_id());
			bool Acquired = false;

			if (m_Atomic.load(std::memory_order_relaxed) == threadId)
			{
				Acquired = true;
			}
			else
			{
				size_t unlockVal = 0;
				Acquired = m_Atomic.compare_exchange_strong(unlockVal, threadId, std::memory_order_relaxed, std::memory_order_relaxed);
			}

			// If the lock was successfully acquired, increment the reference count and ensure memory visibility
			if (Acquired)
			{
				++m_RefCount;
				std::atomic_thread_fence(std::memory_order_acquire);
			}

			return Acquired;
		}

		// Standard lock methods to be used by standard library locking mechanisms like std::unique_lock
		inline void lock() { Acquire(); }
		inline void unlock() { Release(); }
		inline bool try_lock() { return TryAcquire(); }

	private:
		std::atomic<std::size_t> m_Atomic{ 0 };
		uint32_t m_RefCount{ 0 };
	};

}