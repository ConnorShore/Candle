#pragma once

#include "FramePacket.h"

#include <array>
#include <atomic>
#include <mutex>
#include <condition_variable>


namespace Candle {

	constexpr uint32_t kFramePacketCount = 2;

	class FramePump
	{
	public:
		FramePump() = default;
		~FramePump() = default;

		// Delete copy and move constructors and assignment operators
		FramePump(const FramePump&) = delete;
		FramePump& operator=(const FramePump&) = delete;
		FramePump(FramePump&&) = delete;
		FramePump& operator=(FramePump&&) = delete;

		FramePacket& AquireForWriting();
		FramePacket& AquireForReading();

		void Submit(FramePacket& packet);
		void Release(FramePacket& packet);

	private:
		std::array<FramePacket, kFramePacketCount> m_FramePackets;
		std::atomic<uint64_t> m_Published{ 0 }, m_Released{ 0 };
		std::mutex m_Mutex;
		std::condition_variable m_ReadyCV, m_FreeCV;
	};

}