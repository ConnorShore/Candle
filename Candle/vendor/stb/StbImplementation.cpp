// The one TU that compiles stb's implementations; defining STB_IMAGE_IMPLEMENTATION anywhere else links a second copy.
// stb_image's failure reason is thread_local, but stbi_set_flip_vertically_on_load and its siblings set process-wide
// globals: set them once before any worker loads, or use the *_thread variants.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
