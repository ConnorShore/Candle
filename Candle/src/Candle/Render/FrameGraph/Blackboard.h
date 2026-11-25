#pragma once

#include <typeindex>
#include <any>
#include <vector>
#include <utility>

namespace Candle {

	class Blackboard
	{
	public:
		template<typename T, typename... Args> T& Set(Args&&... args);

		template<typename T> [[nodiscard]] T& Get();
		template<typename T> [[nodiscard]] T* TryGet();

		template<typename T> [[nodiscard]] const T& Get() const;
		template<typename T> [[nodiscard]] const T* TryGet() const;

		void Clear();

	private:
		//std::unordered_map<std::type_index, std::any> m_Values;
		std::vector<std::pair<std::type_index, std::any>> m_Values;
	};

}

#include "Blackboard.inl"