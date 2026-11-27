// The one TU that compiles stb's implementations; defining STB_IMAGE_IMPLEMENTATION anywhere else links a second copy.
// stb_image's failure reason is thread_local, but stbi_set_flip_vertically_on_load and its siblings set process-wide
// globals: set them once before any worker loads, or use the *_thread variants.
#include "Candle/Core/Memory/GlobalAllocation.h"

// stb_image refuses to build with only some of these defined
#define STBI_MALLOC(size) Candle::Memory::Allocate(size)
#define STBI_REALLOC(ptr, size) Candle::Memory::Reallocate(ptr, size)
#define STBI_FREE(ptr) Candle::Memory::Free(ptr)

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
