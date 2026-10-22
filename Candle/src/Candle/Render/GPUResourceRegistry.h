#pragma once

#include "GPUHandles.h"

#include <vector>

namespace Candle {

	class GPUResourcePoolBase;
	struct FramePacket;

	// Moves release requests from the main thread to the render thread, and freed slots back, through the frame packet.
	// Threading: RegisterPool before the render thread starts. QueueReleasedSlot, ReturnRecycledSlotsToPools and
	//   MoveReleasedSlotsIntoPacket on the main thread; RetireReleasedSlots and FreeRetiredSlots on the render thread.
	//   The two sides share no state, since FramePump orders every packet hand-off.
	class GPUResourceRegistry
	{
	public:
		GPUResourceRegistry() = default;

		GPUResourceRegistry(const GPUResourceRegistry&) = delete;
		GPUResourceRegistry& operator=(const GPUResourceRegistry&) = delete;

		uint16_t RegisterPool(GPUResourcePoolBase& pool);	// Returns the pool's PoolID
		void QueueReleasedSlot(const GPUSlotRef& slot);	// Held until the next MoveReleasedSlotsIntoPacket

		// Main thread, on a packet just acquired for writing
		void ReturnRecycledSlotsToPools(FramePacket& packet);
		void MoveReleasedSlotsIntoPacket(FramePacket& packet);

		// Render thread, after the submit that recorded the packet and before the packet goes back to the main thread
		void RetireReleasedSlots(const FramePacket& packet, uint64_t retireValue);
		void FreeRetiredSlots(uint64_t completedValue, FramePacket& packet);	// Writes the freed slots into the packet

	private:
		std::vector<GPUResourcePoolBase*> m_Pools;	// Indexed by PoolID
		std::vector<GPUSlotRef> m_PendingReleases;
	};

}
