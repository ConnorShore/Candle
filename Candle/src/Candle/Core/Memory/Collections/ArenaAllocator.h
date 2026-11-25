#pragma once

#include "Candle/Core/Core.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace Candle {

	template<typename T>
	concept IsTrivial = std::is_trivially_destructible_v<T>;

	// A simple arena allocator that allocates memory in a linear fashion and can be reset to free all allocations at once.
	// Data must be trivially destructible as no destructors will ever be called during Reset()
	// Threading: Not synchronized, single user only
	class ArenaAllocator
	{
	public:
		ArenaAllocator(size_t capacity)
			: m_Capacity(capacity), m_Offset(0)
		{
			m_Buffer = new uint8_t[capacity];
		}

		~ArenaAllocator()
		{
			delete[] m_Buffer;
		}

		// Delete copy and move constructors and assignment operators
		ArenaAllocator(const ArenaAllocator&) = delete;
		ArenaAllocator(ArenaAllocator&&) = delete;
		ArenaAllocator& operator=(const ArenaAllocator&) = delete;
		ArenaAllocator& operator=(ArenaAllocator&&) = delete;

		template<IsTrivial T, typename ...Args>
		inline T* New(Args&&... args)
		{
			void* memory = Allocate(sizeof(T), alignof(T));
			if (!memory)
				CDL_CORE_PANIC(LogChannel::Memory, "ArenaAllocator is out of memory: {} more bytes with {} of {} used; raise its capacity", sizeof(T), m_Offset, m_Capacity);

			return ::new (memory) T(std::forward<Args>(args)...);
		}

		inline void Reset()
		{
			m_Offset = 0;
		}

	private:
		void* Allocate(size_t size, size_t alignment)
		{
			uintptr_t currentAddress = reinterpret_cast<uintptr_t>(m_Buffer + m_Offset);
			size_t padding = (alignment - (currentAddress % alignment)) % alignment;
			if (m_Offset + size + padding > m_Capacity)
				return nullptr; // Out of memory

			m_Offset += padding;

			void* ptr = m_Buffer + m_Offset;
			m_Offset += size;
			return ptr;
		}

	private:
		size_t m_Capacity;
		size_t m_Offset;
		uint8_t* m_Buffer;
	};

}