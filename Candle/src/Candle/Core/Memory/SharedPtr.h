#pragma once

#include "Candle/Core/Asserts.h"

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace Candle {

	template<typename T>
	class SharedPtr;

	class SharedResource
	{
	public:
		SharedResource() = default;
		virtual ~SharedResource() = default;

		SharedResource(const SharedResource&) {}
		SharedResource& operator=(const SharedResource&) { return *this; }

		// Debug/diagnostic only: the value may be stale by the time the caller reads it.
		size_t GetRefCount() const { return m_RefCount.load(std::memory_order_relaxed); }

	private:
		template<typename U>
		friend class SharedPtr;

		void AddRef() const { m_RefCount.fetch_add(1, std::memory_order_relaxed); }
		bool Release() const { return m_RefCount.fetch_sub(1, std::memory_order_acq_rel) == 1; }

		mutable std::atomic<size_t> m_RefCount{ 0 };
	};

	template<typename T>
	class SharedPtr
	{
	public:
		SharedPtr() = default;
		SharedPtr(std::nullptr_t) {}
		explicit SharedPtr(T* ptr) : m_Ptr(ptr) { IncrementRefCount(); }
		SharedPtr(const SharedPtr& other) : m_Ptr(other.m_Ptr) { IncrementRefCount(); }
		SharedPtr(SharedPtr&& other) noexcept : m_Ptr(std::exchange(other.m_Ptr, nullptr)) {}

		template<typename U> requires std::is_convertible_v<U*, T*>
		SharedPtr(const SharedPtr<U>& other) : m_Ptr(other.m_Ptr) { IncrementRefCount(); }

		template<typename U> requires std::is_convertible_v<U*, T*>
		SharedPtr(SharedPtr<U>&& other) noexcept : m_Ptr(std::exchange(other.m_Ptr, nullptr)) {}

		~SharedPtr()
		{
			CDL_STATIC_ASSERT(std::is_base_of_v<SharedResource, T>, "SharedPtr<T> requires T to derive from SharedResource");
			DecrementRefCount();
		}

		template<typename... Args>
		static SharedPtr<T> Create(Args&&... args)
		{
			return SharedPtr<T>(new T(std::forward<Args>(args)...));
		}

		T* Get() const { return m_Ptr; }

		void Reset() { SharedPtr().Swap(*this); }
		void Swap(SharedPtr& other) noexcept { std::swap(m_Ptr, other.m_Ptr); }

		SharedPtr& operator=(const SharedPtr& other) { SharedPtr(other).Swap(*this); return *this; }
		SharedPtr& operator=(SharedPtr&& other) noexcept { SharedPtr(std::move(other)).Swap(*this); return *this; }

		template<typename U> requires std::is_convertible_v<U*, T*>
		SharedPtr& operator=(const SharedPtr<U>& other) { SharedPtr(other).Swap(*this); return *this; }

		template<typename U> requires std::is_convertible_v<U*, T*>
		SharedPtr& operator=(SharedPtr<U>&& other) noexcept { SharedPtr(std::move(other)).Swap(*this); return *this; }

		T& operator*() const { return *m_Ptr; }
		T* operator->() const { return m_Ptr; }

		bool operator==(const SharedPtr& other) const { return m_Ptr == other.m_Ptr; }
		explicit operator bool() const { return m_Ptr != nullptr; }

	private:
		template<typename U>
		friend class SharedPtr;

		void IncrementRefCount()
		{
			if (m_Ptr)
				m_Ptr->AddRef();
		}

		void DecrementRefCount()
		{
			if (m_Ptr && m_Ptr->Release())
				delete m_Ptr;
		}

	private:
		T* m_Ptr = nullptr;
	};

	template <typename T, typename U>
	SharedPtr<T> StaticPointerCast(const SharedPtr<U>& ptr)
	{
		return SharedPtr<T>(static_cast<T*>(ptr.Get()));
	}

	template <typename T, typename U>
	SharedPtr<T> DynamicPointerCast(const SharedPtr<U>& ptr)
	{
		if (T* raw = dynamic_cast<T*>(ptr.Get()))
			return SharedPtr<T>(raw);
		return nullptr;
	}

}
