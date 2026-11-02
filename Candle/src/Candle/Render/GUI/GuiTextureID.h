#pragma once

#include "Candle/Render/Resources/GPUHandles.h"

#include <cstdint>

namespace Candle {

	// A texture's ImGui ImTextureID: Index in the low 32 bits, Generation in the high 32, so a handle round-trips through ImGui
	// unchanged. uint64_t rather than ImTextureID so engine headers needn't include imgui; GuiSystem.cpp checks the two match.
	inline uint64_t ToGuiTextureID(GPUTextureHandle handle)
	{
		return (static_cast<uint64_t>(handle.Generation) << 32) | handle.Index;
	}

}
