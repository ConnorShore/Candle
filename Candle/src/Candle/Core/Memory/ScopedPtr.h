#pragma once

#include "Candle/Core/Asserts.h"

#include <type_traits>
#include <utility>

namespace Candle {

	template<typename T>
	class ScopedPtr
	{
	public:
		ScopedPtr() = default;
		explicit ScopedPtr(T* ptr) : m_Ptr(ptr) {}

		ScopedPtr(ScopedPtr&& other) noexcept : m_Ptr(other.Release()) {}

		template<typename U> requires std::is_convertible_v<U*, T*>
		ScopedPtr(ScopedPtr<U>&& other) noexcept : m_Ptr(other.Release())
		{
			CDL_STATIC_ASSERT(std::has_virtual_destructor_v<T> || std::is_same_v<std::remove_cv_t<T>, std::remove_cv_t<U>>,
				"ScopedPtr<Base> from ScopedPtr<Derived> requires Base to have a virtual destructor");
		}

		ScopedPtr(const ScopedPtr&) = delete;

		template<typename U>
		ScopedPtr(const ScopedPtr<U>&) = delete;

		~ScopedPtr() { Reset(); }

		void Reset(T* ptr = nullptr)
		{
			CDL_STATIC_ASSERT(sizeof(T) > 0, "ScopedPtr cannot delete an incomplete type");
			T* old = std::exchange(m_Ptr, ptr);
			delete old;
		}

		[[nodiscard]] T* Release() { return std::exchange(m_Ptr, nullptr); }

		template<typename... Args>
		static ScopedPtr<T> Create(Args&&... args)
		{
			return ScopedPtr<T>(new T(std::forward<Args>(args)...));
		}

		T* Get() const { return m_Ptr; }

		ScopedPtr& operator=(const ScopedPtr& ptr) = delete;

		ScopedPtr& operator=(ScopedPtr&& ptr) noexcept { Reset(ptr.Release()); return *this; }

		template<typename U> requires std::is_convertible_v<U*, T*>
		ScopedPtr& operator=(ScopedPtr<U>&& ptr) noexcept { return *this = ScopedPtr(std::move(ptr)); }

		T& operator*() const { return *m_Ptr; }
		T* operator->() const { return m_Ptr; }

		bool operator==(const ScopedPtr& other) const { return m_Ptr == other.m_Ptr; }
		explicit operator bool() const { return m_Ptr != nullptr; }

	private:
		template <typename U>
		friend class ScopedPtr;

		T* m_Ptr = nullptr;
	};

	// Non-owning: the ScopedPtr keeps ownership, so the returned pointer must not outlive it.
	template <typename T, typename U>
	T* StaticPointerCast(const ScopedPtr<U>& ptr)
	{
		return static_cast<T*>(ptr.Get());
	}
}
