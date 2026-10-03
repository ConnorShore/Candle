#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

namespace Candle {

	// This is a simple placeholder concurrent queue for the job system.
	// This is to get the job system working and verify that the job system is working correctly.
	// Once thats verified this will be replaced with a Chase-Lev deque or a similar concurrent queue implementation.
	template<typename T>
	class SimpleConcurrentQueue
	{
	public:
		SimpleConcurrentQueue() = default;
		~SimpleConcurrentQueue() = default;

		inline void Push(const T& item)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Queue.push(item);
			m_CV.notify_one();
		}

		inline T Pop()
		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			m_CV.wait(lock, [this] { return !m_Queue.empty(); });
			T item = m_Queue.front();
			m_Queue.pop();
			return item;
		}

		inline std::optional<T> TryPop()
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (m_Queue.empty())
				return std::nullopt;
			T item = m_Queue.front();
			m_Queue.pop();
			return item;
		}

		inline bool IsEmpty()
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			return m_Queue.empty();
		}

	private:
		std::queue<T> m_Queue;
		std::mutex m_Mutex;
		std::condition_variable m_CV;
	};

}