#pragma once

#include "Candle/Core/Memory/Collections/ArenaAllocator.h"

#include <typeindex>
#include <type_traits>
#include <vector>
#include <utility>

namespace Candle {

	// Values live in the arena, so a reference from Get stays valid until Clear. Whoever resets the arena clears this with it.
	class Blackboard
	{
	public:
		explicit Blackboard(ArenaAllocator& arena) : m_Arena(arena) {}

		template<typename T, typename... Args> T& Set(Args&&... args);

		template<typename T> [[nodiscard]] T& Get();
		template<typename T> [[nodiscard]] T* TryGet();

		template<typename T> [[nodiscard]] const T& Get() const;
		template<typename T> [[nodiscard]] const T* TryGet() const;

		void Clear();

	private:
		ArenaAllocator& m_Arena;
		std::vector<std::pair<std::type_index, void*>> m_Values;	// Clear keeps its capacity, so steady-state frames don't allocate
	};

}

#include "Blackboard.inl"