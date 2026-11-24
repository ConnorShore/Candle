#include "Candle/Core/Core.h"

namespace Candle {

	template<typename T, typename... Args>
	inline T& Blackboard::Set(Args&&... args)
	{
		m_Values[std::type_index(typeid(T))] = T(std::forward<Args>(args)...);
		return std::any_cast<T&>(m_Values[std::type_index(typeid(T))]);
	}

	template<typename T>
	inline T& Blackboard::Get()
	{
		CDL_CORE_ASSERT(m_Values.find(std::type_index(typeid(T))) != m_Values.end(), "Blackboard does not contain a value of type {}", typeid(T).name());
		return std::any_cast<T&>(m_Values[std::type_index(typeid(T))]);
	}

	template<typename T>
	inline T* Blackboard::TryGet()
	{
		auto it = m_Values.find(std::type_index(typeid(T)));
		if (it != m_Values.end())
			return &std::any_cast<T&>(it->second);
		else
			return nullptr;
	}

	template<typename T>
	inline 	T& Blackboard::Get() const
	{
		CDL_CORE_ASSERT(m_Values.find(std::type_index(typeid(T))) != m_Values.end(), "Blackboard does not contain a value of type {}", typeid(T).name());
		return std::any_cast<T&>(m_Values.at(std::type_index(typeid(T))));
	}

	template<typename T>
	inline T* Blackboard::TryGet() const
	{
		auto it = m_Values.find(std::type_index(typeid(T)));
		if (it != m_Values.end())
			return &std::any_cast<T&>(it->second);
		else
			return nullptr;
	}

	inline void Blackboard::Clear()
	{
		m_Values.clear();
	}

}