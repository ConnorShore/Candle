#include "cdlpch.h"
#include "FramePump.h"

namespace Candle {

	FramePacket& FramePump::AquireForWriting()
	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		CDL_CORE_ASSERT(!m_Closed, "FramePump::AquireForWriting after Close; nothing will read the packet");
		m_FreeCV.wait(lock, [this] {
			return (m_Published.load(std::memory_order_acquire) -
					m_Released.load(std::memory_order_acquire)) < kFramePacketCount;
		});

		const uint64_t writeIndex = m_Published.load(std::memory_order_relaxed) % kFramePacketCount;
		return m_FramePackets[writeIndex];
	}

	FramePacket* FramePump::AquireForReading()
	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		m_ReadyCV.wait(lock, [this] {
			return m_Closed || m_Published.load(std::memory_order_acquire) >
				   m_Released.load(std::memory_order_acquire);
		});

		if (m_Closed)
			return nullptr;

		const uint64_t readIndex = m_Released.load(std::memory_order_relaxed) % kFramePacketCount;
		return &m_FramePackets[readIndex];
	}

	void FramePump::Submit(FramePacket& packet)
	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		m_Published.fetch_add(1, std::memory_order_release);
		m_ReadyCV.notify_one();
	}

	void FramePump::Release(FramePacket& packet)
	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		m_Released.fetch_add(1, std::memory_order_release);
		m_FreeCV.notify_one();
	}

	void FramePump::Close()
	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		m_Closed = true;
		m_ReadyCV.notify_all();
	}

}