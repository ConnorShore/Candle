#pragma once

#include "Candle/Render/Resources/GPUHandles.h"

#include "Candle/Core/Asserts.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <vector>
#include <span>
#include <concepts>

namespace Candle {

	class GPUResourceRegistry;

	struct GPUResourcePoolStats
	{
		uint64_t SizeBytes{ 0 };	// The live slots' GPU memory. A released slot stops counting at once, though its memory lasts until the GPU retires it
		uint64_t TotalReleases{ 0 };	// Only ever rises, so two captures can be diffed for a rate
		uint32_t TotalSlots{ 0 };
		uint32_t LiveSlots{ 0 };
		uint32_t PendingSlots{ 0 };	// Released but not back on the free list yet: riding a packet, or waiting on the GPU
		uint32_t FreeSlots{ 0 };	// The free list plus slots never used: what Insert can still take
		uint32_t MaxUsedSlots{ 0 };	// The most slots ever live or pending at once. The pool panics if it needs more than TotalSlots
	};

	// Handle bookkeeping shared by every pool; GPUResourcePool<T, Tag> adds the payloads.
	// Threading: AllocateSlot, ReleaseSlot, IsSlotAlive, RecycleSlot, CaptureStats and GetLiveHandles on the main thread; SlotHoldsGeneration, RetireSlot
	//   and FreeRetiredSlots on the render thread. The main thread fills a slot before its handle is first published, the
	//   render thread reads it and destroys its payload once the GPU has retired it, and the index returns to the main
	//   thread through RecycleSlot. Destroying a pool frees every payload it still holds, so the GPU must be idle by then.
	class GPUResourcePoolBase
	{
	public:
		virtual ~GPUResourcePoolBase() = default;

		GPUResourcePoolBase(const GPUResourcePoolBase&) = delete;
		GPUResourcePoolBase& operator=(const GPUResourcePoolBase&) = delete;

		void RetireSlot(uint32_t index, uint32_t generation, uint64_t retireValue);	// Freed once the GPU reaches retireValue
		void FreeRetiredSlots(uint64_t completedValue, std::vector<GPUSlotRef>& freedSlots);
		void RecycleSlot(uint32_t index);	// Back on the free list

		GPUResourcePoolStats CaptureStats() const;

		uint32_t GetCapacity() const { return static_cast<uint32_t>(m_Handles.size()); }

	protected:
		GPUResourcePoolBase(GPUResourceRegistry& registry, uint32_t capacity);

		GPUSlotRef AllocateSlot(uint64_t sizeBytes);
		void ReleaseSlot(uint32_t index, uint32_t generation);
		bool IsSlotAlive(uint32_t index, uint32_t generation) const;
		bool SlotHoldsGeneration(uint32_t index, uint32_t generation) const;	// The render thread's view of whether the slot still holds that resource

		virtual void DestroyPayload(uint32_t index) = 0;

		inline std::vector<std::pair<uint32_t, uint32_t>> GetLiveHandles() const
		{
			std::vector<std::pair<uint32_t, uint32_t>> liveHandles;
			liveHandles.reserve(m_Handles.size());
			for (uint32_t i = 0; i < m_Handles.size(); ++i)
			{
				if (m_Handles[i].Live)
					liveHandles.push_back({ i, m_SlotGenerations[i] });
			}
			return liveHandles;
		}

	private:
		struct HandleState
		{
			uint32_t Generation{ 0 };
			bool Live{ false };
			uint64_t SizeBytes{ 0 };
		};

		struct RetiringSlot
		{
			uint32_t Index;
			uint64_t RetireValue;
		};

	private:
		GPUResourceRegistry& m_Registry;
		uint16_t m_PoolID{ 0 };

		std::vector<uint32_t> m_SlotGenerations;	// Copied at allocation, so the render thread never reads m_Handles. Never resized.

		// Main thread only
		std::vector<HandleState> m_Handles;
		std::vector<uint32_t> m_FreeIndices;
		uint32_t m_NextUnused{ 0 };
		uint32_t m_LiveCount{ 0 };
		uint64_t m_LiveBytes{ 0 };	// Kept as a total since the main thread can't walk the payloads: the render thread destroys them
		uint64_t m_TotalReleases{ 0 };

		// Render thread only
		std::deque<RetiringSlot> m_RetiringSlots;
	};

	// Ensure the payload type implements GetSizeBytes
	template<typename T>
	concept GPUResourcePayload = requires(const T & payload) { { payload.GetSizeBytes() } -> std::convertible_to<uint64_t>; };

	template<GPUResourcePayload T, typename Tag>
	class GPUResourcePool final : public GPUResourcePoolBase
	{
	public:
		using Handle = GPUHandle<Tag>;

		inline GPUResourcePool(GPUResourceRegistry& registry, uint32_t capacity)
			: GPUResourcePoolBase(registry, capacity)
			, m_Payloads(capacity)
		{
		}

		inline Handle Insert(T&& payload)
		{
			const GPUSlotRef slot = AllocateSlot(payload.GetSizeBytes());
			m_Payloads[slot.Index].emplace(std::move(payload));
			return { .Index = slot.Index, .Generation = slot.Generation };
		}

		// The handle dies at once, its payload lives until the GPU retires the frame that carried the release
		inline void Release(Handle handle) { ReleaseSlot(handle.Index, handle.Generation); }
		inline bool IsAlive(Handle handle) const { return IsSlotAlive(handle.Index, handle.Generation); }

		inline const T& Get(Handle handle) const
		{
			CDL_CORE_ASSERT(SlotHoldsGeneration(handle.Index, handle.Generation) && m_Payloads[handle.Index], "Get was given an invalid or stale handle");
			return *m_Payloads[handle.Index];
		}

		// Get without the assert: null for an invalid or stale handle, for callers with a fallback
		inline const T* TryGet(Handle handle) const
		{
			if (!SlotHoldsGeneration(handle.Index, handle.Generation) || !m_Payloads[handle.Index])
				return nullptr;

			return &*m_Payloads[handle.Index];
		}

		inline std::vector<Handle> GetLiveHandles() const
		{
			const auto livePairs = GPUResourcePoolBase::GetLiveHandles();

			std::vector<Handle> liveHandles;
			liveHandles.reserve(livePairs.size());
			for (const auto& [index, generation] : livePairs)
			{
				Handle handle{ .Index = index, .Generation = generation };
				liveHandles.push_back({ .Index = index, .Generation = generation });
			}

			return std::move(liveHandles);
		}

	protected:
		inline void DestroyPayload(uint32_t index) override { m_Payloads[index].reset(); }

	private:
		std::vector<std::optional<T>> m_Payloads;	// Never resized: the render thread reads it while the main thread inserts
	};

}
