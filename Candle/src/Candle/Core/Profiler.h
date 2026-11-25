#pragma once

#ifdef CDL_PROFILE
#include <tracy/Tracy.hpp>
#include <cstring>
#endif

#if CDL_PROFILE_ENABLED
	constexpr tracy::Color::ColorType kWaitColor = tracy::Color::ColorType::Red;
	constexpr uint8_t kAllocStackDepth = 16;

	// Time macros
	#define CDL_PROFILE_SCOPE(name) ZoneScopedN(name)
	#define CDL_PROFILE_SCOPE_METADATA(value) ZoneValue(value)
	// Renames the enclosing scope at runtime, for names only known at runtime (e.g. JobSpec::Name); Tracy copies the string.
	#define CDL_PROFILE_SCOPE_NAME(name) ZoneName(name, std::strlen(name))
	// Attaches text to the enclosing scope without renaming it, shown in the zone's tooltip; Tracy copies the string.
	#define CDL_PROFILE_SCOPE_TEXT(text) ZoneText(text, std::strlen(text))
	#define CDL_PROFILE_FUNCTION() CDL_PROFILE_SCOPE(__FUNCSIG__)
	#define CDL_PROFILE_FRAME() FrameMark
	#define CDL_PROFILE_FRAME_NAMED(name) FrameMarkNamed(name)
	#define CDL_PROFILE_THREAD(name) tracy::SetThreadName(name)
	#define CDL_PROFILE_MESSAGE(message) TracyMessageL(message)
	#define CDL_PROFILE_WAIT(name) ZoneScopedNC(name, kWaitColor)

	// Memory macros
	#define CDL_PROFILE_ALLOC(ptr, size) TracyAlloc(ptr, size)
	#define CDL_PROFILE_FREE(ptr) TracyFree(ptr)
	#define CDL_PROFILE_ALLOC_STACK(ptr, size) TracyAllocS(ptr, size, kAllocStackDepth)
	#define CDL_PROFILE_FREE_STACK(ptr) TracyFreeS(ptr, kAllocStackDepth)

#else
	#define CDL_PROFILE_SCOPE(name)
	#define CDL_PROFILE_SCOPE_METADATA(value)
	#define CDL_PROFILE_SCOPE_NAME(name)
	#define CDL_PROFILE_SCOPE_TEXT(text)
	#define CDL_PROFILE_FUNCTION()
	#define CDL_PROFILE_FRAME()
	#define CDL_PROFILE_FRAME_NAMED(name)
	#define CDL_PROFILE_THREAD(name)
	#define CDL_PROFILE_MESSAGE(message)
	#define CDL_PROFILE_WAIT(name)

	#define CDL_PROFILE_ALLOC(ptr, size)
	#define CDL_PROFILE_FREE(ptr)
	#define CDL_PROFILE_ALLOC_STACK(ptr, size)
	#define CDL_PROFILE_FREE_STACK(ptr)
#endif
