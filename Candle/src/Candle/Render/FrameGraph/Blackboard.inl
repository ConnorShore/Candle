#include "Candle/Core/Core.h"

namespace Candle {

	template<typename T, typename... Args>
	inline T& Blackboard::Set(Args&&... args)
	{
		CDL_STATIC_ASSERT(std::is_trivially_destructible_v<T>, "Blackboard values live in the frame arena, which never runs destructors");

		// Overwritten in place, so references already handed out see the new value
		if (T* existing = TryGet<T>())
		{
			*existing = T(std::forward<Args>(args)...);
			return *existing;
		}

		T* value = m_Arena.New<T>(std::forward<Args>(args)...);
		m_Values.emplace_back(std::type_index(typeid(T)), value);
		return *value;
	}

	template<typename T>
	inline T& Blackboard::Get()
	{
		if (auto* value = TryGet<T>())
			return *value;

		CDL_CORE_PANIC(LogChannel::Render, "Blackboard does not contain a value of type {}", typeid(T).name());
	}

	template<typename T>
	inline T* Blackboard::TryGet()
	{
		for (const auto& [type, value] : m_Values)
		{
			if (type == std::type_index(typeid(T)))
				return static_cast<T*>(value);
		}

		return nullptr;
	}

	template<typename T>
	inline const T& Blackboard::Get() const
	{
		if (auto* value = TryGet<T>())
			return *value;
		
		CDL_CORE_PANIC(LogChannel::Render, "Blackboard does not contain a value of type {}", typeid(T).name());
	}

	template<typename T>
	inline const T* Blackboard::TryGet() const
	{
		for (const auto& [type, value] : m_Values)
		{
			if (type == std::type_index(typeid(T)))
				return static_cast<const T*>(value);
		}

		return nullptr;
	}

	inline void Blackboard::Clear()
	{
		m_Values.clear();
	}

}