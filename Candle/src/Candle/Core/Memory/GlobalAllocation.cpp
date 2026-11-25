#include "cdlpch.h"
#include "GlobalAllocation.h"

#include <cstdlib>
#include <malloc.h>
#include <new>

namespace Candle::Memory {

	void* Allocate(size_t size)
	{
		void* ptr = std::malloc(size);
		if (ptr)
		{
			CDL_PROFILE_ALLOC_STACK(ptr, size);
		}
		return ptr;
	}

	void* AllocateZeroed(size_t count, size_t size)
	{
		void* ptr = std::calloc(count, size);
		if (ptr)
		{
			CDL_PROFILE_ALLOC_STACK(ptr, count * size);
		}
		return ptr;
	}

	void* Reallocate(void* ptr, size_t size)
	{
		if (!ptr)
			return Allocate(size);

		if (size == 0)
		{
			Free(ptr);
			return nullptr;
		}

		CDL_PROFILE_FREE_STACK(ptr);
		void* result = std::realloc(ptr, size);
		CDL_PROFILE_ALLOC_STACK(result ? result : ptr, result ? size : _msize(ptr));	// A failed realloc leaves ptr allocated as it was
		return result;
	}

	void Free(void* ptr)
	{
		if (!ptr)
			return;

		CDL_PROFILE_FREE_STACK(ptr);
		std::free(ptr);
	}

	void* AllocateAligned(size_t size, size_t alignment)
	{
		void* ptr = _aligned_malloc(size, alignment);
		if (ptr)
		{
			CDL_PROFILE_ALLOC_STACK(ptr, size);
		}
		return ptr;
	}

	void FreeAligned(void* ptr)
	{
		if (!ptr)
			return;

		CDL_PROFILE_FREE_STACK(ptr);
		_aligned_free(ptr);
	}

}

#if CDL_PROFILE_ENABLED

// MSVC's sized and nothrow forms forward to these, so they're tracked without overloads of their own
void* operator new(std::size_t count)
{
	void* ptr = Candle::Memory::Allocate(count);
	if (!ptr) throw std::bad_alloc();
	return ptr;
}

void operator delete(void* ptr) noexcept
{
	Candle::Memory::Free(ptr);
}

void* operator new[](std::size_t count)
{
	return ::operator new(count);
}

void operator delete[](void* ptr) noexcept
{
	return ::operator delete(ptr);
}

void* operator new(std::size_t count, std::align_val_t alignment)
{
	void* ptr = Candle::Memory::AllocateAligned(count, static_cast<std::size_t>(alignment));
	if (!ptr) throw std::bad_alloc();
	return ptr;
}

void operator delete(void* ptr, std::align_val_t) noexcept
{
	Candle::Memory::FreeAligned(ptr);
}

void* operator new[](std::size_t count, std::align_val_t alignment)
{
	return ::operator new(count, alignment);
}

void operator delete[](void* ptr, std::align_val_t alignment) noexcept
{
	return ::operator delete(ptr, alignment);
}

#endif