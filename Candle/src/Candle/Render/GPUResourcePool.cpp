#include "cdlpch.h"
#include "GPUResourcePool.h"
#include "GPUResourceRegistry.h"

#include "Candle/Platform/Platform.h"

namespace Candle {

	GPUResourcePoolBase::GPUResourcePoolBase(GPUResourceRegistry& registry, uint32_t capacity)
		: m_Registry(registry)
		, m_SlotGenerations(capacity)
		, m_Handles(capacity)
	{
		m_FreeIndices.reserve(capacity);
		m_PoolID = m_Registry.RegisterPool(*this);
	}

	void GPUResourcePoolBase::RetireSlot(uint32_t index, [[maybe_unused]] uint32_t generation, uint64_t retireValue)
	{
		CDL_CORE_ASSERT(SlotHoldsGeneration(index, generation), "Retiring a slot that holds a different resource");
		CDL_CORE_ASSERT(m_RetiringSlots.empty() || m_RetiringSlots.back().RetireValue <= retireValue, "Retire values must not decrease");
		m_RetiringSlots.push_back({ .Index = index, .RetireValue = retireValue });
	}

	void GPUResourcePoolBase::FreeRetiredSlots(uint64_t completedValue, std::vector<GPUSlotRef>& freedSlots)
	{
		// Retire values only increase, so everything the GPU has finished with is at the front
		while (!m_RetiringSlots.empty() && m_RetiringSlots.front().RetireValue <= completedValue)
		{
			const uint32_t index = m_RetiringSlots.front().Index;
			DestroyPayload(index);
			freedSlots.push_back({ .PoolID = m_PoolID, .Index = index });
			m_RetiringSlots.pop_front();
		}
	}

	void GPUResourcePoolBase::RecycleSlot(uint32_t index)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GPUResourcePool slots are recycled on the main thread");
		m_FreeIndices.push_back(index);
	}

	GPUSlotRef GPUResourcePoolBase::AllocateSlot()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GPUResourcePool slots are allocated on the main thread");

		uint32_t index;
		if (!m_FreeIndices.empty())
		{
			index = m_FreeIndices.back();
			m_FreeIndices.pop_back();
		}
		else
		{
			// Fatal in every configuration, since Dist compiles asserts out and would write past the end
			if (m_NextUnused == m_Handles.size())
				CDL_CORE_PANIC(LogChannel::Render, "GPUResourcePool is full at {} slots; raise its capacity", m_Handles.size());
			index = m_NextUnused++;
		}

		HandleState& state = m_Handles[index];
		state.Live = true;
		m_SlotGenerations[index] = state.Generation;

		return { .PoolID = m_PoolID, .Index = index, .Generation = state.Generation };
	}

	void GPUResourcePoolBase::ReleaseSlot(uint32_t index, uint32_t generation)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GPUResourcePool slots are released on the main thread");
		CDL_CORE_ASSERT(IsSlotAlive(index, generation), "Release was given an invalid or already released handle");

		HandleState& state = m_Handles[index];
		state.Live = false;
		++state.Generation;
		m_Registry.QueueReleasedSlot({ .PoolID = m_PoolID, .Index = index, .Generation = generation });
	}

	bool GPUResourcePoolBase::IsSlotAlive(uint32_t index, uint32_t generation) const
	{
		return index < m_Handles.size() && m_Handles[index].Live && m_Handles[index].Generation == generation;
	}

	bool GPUResourcePoolBase::SlotHoldsGeneration(uint32_t index, uint32_t generation) const
	{
		return index < m_SlotGenerations.size() && m_SlotGenerations[index] == generation;
	}

}
