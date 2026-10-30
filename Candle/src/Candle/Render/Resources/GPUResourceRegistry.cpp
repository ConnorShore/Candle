#include "cdlpch.h"
#include "GPUResourceRegistry.h"
#include "Candle/Render/Resources/GPUResourcePool.h"
#include "Candle/Render/FramePacket.h"

#include "Candle/Platform/Platform.h"

namespace Candle {

	uint16_t GPUResourceRegistry::RegisterPool(GPUResourcePoolBase& pool)
	{
		CDL_CORE_ASSERT(m_Pools.size() < std::numeric_limits<uint16_t>::max(), "Too many GPU resource pools");
		m_Pools.push_back(&pool);
		return static_cast<uint16_t>(m_Pools.size() - 1);
	}

	void GPUResourceRegistry::QueueReleasedSlot(const GPUSlotRef& slot)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GPU resource releases are queued on the main thread");
		m_PendingReleases.push_back(slot);
	}

	void GPUResourceRegistry::ReturnRecycledSlotsToPools(FramePacket& packet)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Recycled slots are returned on the main thread");

		for (const GPUSlotRef& slot : packet.RecycledSlots)
			m_Pools[slot.PoolID]->RecycleSlot(slot.Index);
		packet.RecycledSlots.clear();
	}

	void GPUResourceRegistry::MoveReleasedSlotsIntoPacket(FramePacket& packet)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "Released slots are moved into packets on the main thread");

		// Swapped rather than moved, so both vectors keep their capacity from frame to frame
		packet.ReleasedSlots.clear();
		std::swap(packet.ReleasedSlots, m_PendingReleases);
	}

	void GPUResourceRegistry::RetireReleasedSlots(const FramePacket& packet, uint64_t retireValue)
	{
		for (const GPUSlotRef& slot : packet.ReleasedSlots)
			m_Pools[slot.PoolID]->RetireSlot(slot.Index, slot.Generation, retireValue);
	}

	void GPUResourceRegistry::FreeRetiredSlots(uint64_t completedValue, FramePacket& packet)
	{
		for (GPUResourcePoolBase* pool : m_Pools)
			pool->FreeRetiredSlots(completedValue, packet.RecycledSlots);
	}

}
