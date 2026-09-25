#pragma once

#include "Candle/Core/Core.h"

#include <condition_variable>
#include <mutex>

namespace Candle {

	class AdaptiveSpinLock
	{
	public:
		inline bool TryAquire()
		{
			return !m_Flag.test_and_set(std::memory_order_acquire);
		}

		inline void Aquire()
		{
			uint8_t spinCount = 0;
			while (spinCount < s_SpinThreshold)
			{
				if (!m_Flag.test_and_set(std::memory_order_acquire))
					return; // Lock acquired

				CDL_THREAD_PAUSE();
				spinCount++;
			}

			// Spin threshold reached; acquire the fallback mutex and park the thread
			std::unique_lock<std::mutex> lock(m_Mutex);
			while (m_Flag.test_and_set(std::memory_order_acquire)) {
				// Wait until notified that the lock is released
				m_CV.wait(lock);
			}
		}

		inline void Release()
		{
			m_Flag.clear(std::memory_order_release);

			std::lock_guard<std::mutex> lk(m_Mutex);
			m_CV.notify_one();
		}


		// Standard lock methods to be used by standard library locking mechanisms like std::unique_lock
		inline void lock() { Aquire(); }
		inline void unlock() { Release(); }
		inline bool try_lock() { return TryAquire(); }

	private:
		std::atomic_flag m_Flag = ATOMIC_FLAG_INIT;
		std::mutex m_Mutex;
		std::condition_variable m_CV;

		inline static constexpr size_t s_SpinThreshold = 2048; // Number of spins before falling back to std::mutex
	};

}