#pragma once

#include <atomic>
#include <thread>

#include "Candle/Core/Core.h"

namespace Candle {

	class SpinLock
	{
	public:
		SpinLock() = default;
		~SpinLock() = default;

		inline bool TryAquire()
		{
			return !m_Flag.test_and_set(std::memory_order_acquire);
		}

		inline void Aquire()
		{
			while (m_Flag.test_and_set(std::memory_order_acquire))
			{
				// Spin until the lock is acquired
				CDL_THREAD_PAUSE();
			}
		}

		inline void Release()
		{
			m_Flag.clear(std::memory_order_release);
		}

		// Standard lock methods to be used by standard library locking mechanisms like std::unique_lock
		inline void lock() { Aquire(); }
		inline void unlock() { Release(); }
		inline bool try_lock() { return TryAquire(); }

	private:
		std::atomic_flag m_Flag = ATOMIC_FLAG_INIT;
	};

}