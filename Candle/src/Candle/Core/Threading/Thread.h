#pragma once

#include "Candle/Platform/Platform.h"
#include "ThreadPriority.h"

#include <thread>
#include <string>
#include <concepts>
#include <stop_token>
#include <type_traits>

namespace Candle {

	struct ThreadDescriptor
	{
		std::string Name;
		ThreadPriority Priority = ThreadPriority::Normal;
		uint64_t AffinityMask = 0;	// 0 means no affinity mask is set, allowing the OS to schedule the thread on any available core
	};

	// Define a concept to check if a function is invocable with a stop_token and the provided arguments
	// We use decay_t to remove cv-qualifiers (const and volitile) and convert references to values for the function signature check
	template <typename Func, typename... Args>
	concept ThreadFunc =
		std::invocable<std::decay_t<Func>&, std::stop_token, std::decay_t<Args>...> ||
		std::invocable<std::decay_t<Func>&, std::decay_t<Args>...>;

	class Thread
	{
	public:
		explicit Thread(const ThreadDescriptor& descriptor) : m_Descriptor(descriptor) {}
		explicit Thread(const std::string& name) : m_Descriptor({ name }) {}

		// Delete copy constructor and assignment operator to prevent copying
		Thread(const Thread&) = delete;
		Thread& operator=(const Thread&) = delete;

		~Thread() = default;

		template<typename Func, typename... Args>
			requires ThreadFunc<Func, Args...>
		void Start(Func&& func, Args&&... args)
		{
			m_Thread = std::jthread([this, func = std::forward<Func>(func)](std::stop_token stoken, std::decay_t<Args>... fargs) mutable {
				if (!m_Descriptor.Name.empty())
					Platform::SetCurrentThreadName(m_Descriptor.Name.c_str());
				if (m_Descriptor.AffinityMask != 0)
					Platform::SetCurrentThreadAffinityMask(m_Descriptor.AffinityMask);

				Platform::SetCurrentThreadPriority(m_Descriptor.Priority);

				// Call the function with or without the stop_token based on its signature
				if constexpr (std::invocable<std::decay_t<Func>&, std::stop_token, std::decay_t<Args>...>)
					func(std::move(stoken), std::move(fargs)...);
				else
					func(std::move(fargs)...);
				}, std::forward<Args>(args)...);
		}

		void Join() { if (m_Thread.joinable()) m_Thread.join(); }
		void StopRequest() { m_Thread.request_stop(); }

		uint32_t GetId() { return Platform::GetThreadId(m_Thread.native_handle()); }

	private:
		ThreadDescriptor m_Descriptor;
		std::jthread m_Thread;
	};

}