#pragma once

#include <cstddef>

// Self-contained so the vendor projects (stb, VMA) can route their allocations here without the engine's pch.
namespace Candle::Memory {

	// Where global new/delete and the third-party allocator hooks end up, so the profiler sees all of it.
	// Allocate, AllocateZeroed and Reallocate pair with Free; AllocateAligned pairs with FreeAligned. Crossing them corrupts the heap.
	// Threading: any thread.
	void* Allocate(size_t size);
	void* AllocateZeroed(size_t count, size_t size);
	void* Reallocate(void* ptr, size_t size);		// Null ptr allocates; a zero size frees and returns null
	void Free(void* ptr);

	void* AllocateAligned(size_t size, size_t alignment);	// alignment must be a power of two
	void FreeAligned(void* ptr);

}
