#include "cdlpch.h"
#include "MainThreadLogSink.h"

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
		for (const auto& [handle, listener] : m_Listeners)
		{
			while (true)
			{
				LogRecord batch[64];
				const size_t count = m_Pending.PopBatch(batch, 64);
				if (count == 0)
					break;
				listener(std::span<const LogRecord>(batch, count));
			}
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