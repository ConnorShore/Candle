#pragma once

#include "Candle/Render/FramePacket.h"

#include <array>
#include <atomic>
#include <mutex>
#include <condition_variable>


namespace Candle {

	constexpr uint32_t kFramePacketCount = 2;

	// Hands frame packets from the main thread to the render thread in order, at most kFramePacketCount at a time.
	// Threading: AquireForWriting, Submit and Close on the main thread; AquireForReading and Release on the render thread.
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
		FramePacket* AquireForReading();	// Blocks for the next packet; nullptr once the pump is closed, even with packets unread

		void Submit(FramePacket& packet);
		void Release(FramePacket& packet);

		void Close();	// Shutdown: wakes a reader waiting for a packet that will never be submitted

	private:
		std::array<FramePacket, kFramePacketCount> m_FramePackets;
		std::atomic<uint64_t> m_Published{ 0 }, m_Released{ 0 };
		bool m_Closed{ false };	// Guarded by m_Mutex
		std::mutex m_Mutex;
		std::condition_variable m_ReadyCV, m_FreeCV;
	};

}