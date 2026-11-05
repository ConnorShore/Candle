#include "cdlpch.h"
#include "MainThreadLogSink.h"

#include "Candle/Core/Profiler.h"

namespace Candle {

	MainThreadLogSink::MainThreadLogSink(size_t capacity)
		: m_Pending(capacity)
	{
	}

	void MainThreadLogSink::Write(const LogRecord* records, size_t count)
	{
		m_Pending.TryPushN(records, count);
	}

	void MainThreadLogSink::Dispatch()
	{
		CDL_PROFILE_SCOPE("MainThreadLogSink::Dispatch");

		// Left pending, so startup's records reach the first listener to register
		if (m_Listeners.empty())
			return;

		LogRecord batch[64];
		while (const size_t count = m_Pending.PopBatch(batch, 64))
		{
			for (const auto& [handle, listener] : m_Listeners)
				listener(std::span<const LogRecord>(batch, count));
		}
	}

	LogListenerHandle MainThreadLogSink::RegisterListener(LogListener listener)
	{
		m_Listeners.emplace_back(m_NextHandle, std::move(listener));
		return m_NextHandle++;
	}

	void MainThreadLogSink::UnregisterListener(LogListenerHandle handle)
	{
		std::erase_if(m_Listeners, [handle](const auto& pair) { return pair.first == handle; });
	}

}