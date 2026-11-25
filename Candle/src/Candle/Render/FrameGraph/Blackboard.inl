#include "Candle/Core/Core.h"

namespace Candle {

	template<typename T, typename... Args>
	inline T& Blackboard::Set(Args&&... args)
	{
		m_Values.emplace_back(std::type_index(typeid(T)), T(std::forward<Args>(args)...));
		return std::any_cast<T&>(m_Values.back().second);
	}

	template<typename T>
	inline T& Blackboard::Get()
	{
		if (auto* value = TryGet<T>())
			return *value;
		else
			CDL_CORE_ASSERT(false, "Blackboard does not contain a value of type {}", typeid(T).name());
	}

	template<typename T>
	inline T* Blackboard::TryGet()
	{
		for (auto& [type, value] : m_Values)
		{
			if (type == std::type_index(typeid(T)))
				return &std::any_cast<T&>(value);
		}
	}

	template<typename T>
	inline const T& Blackboard::Get() const
	{
		if (auto* value = TryGet<T>())
			return *value;
		else
			CDL_CORE_ASSERT(false, "Blackboard does not contain a value of type {}", typeid(T).name());
	}

	template<typename T>
	inline const T* Blackboard::TryGet() const
	{
		for (const auto& [type, value] : m_Values)
		{
			if (type == std::type_index(typeid(T)))
				return &std::any_cast<const T&>(value);
		}
	}

	inline void Blackboard::Clear()
	{
		m_Values.clear();
	}

}