// The one TU that compiles VMA's implementation; defining VMA_IMPLEMENTATION anywhere else links a second copy.
#include "Candle/Core/Memory/GlobalAllocation.h"

// Macros rather than VmaAllocatorCreateInfo::pAllocationCallbacks: VMA forwards those into its vkAllocateMemory and
// vkCreateBuffer/Image calls, which would route the driver's allocations through Candle too and tie their destruction to them.
#define VMA_SYSTEM_ALIGNED_MALLOC(size, alignment) Candle::Memory::AllocateAligned((size), (alignment))
#define VMA_SYSTEM_ALIGNED_FREE(ptr) Candle::Memory::FreeAligned(ptr)

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
