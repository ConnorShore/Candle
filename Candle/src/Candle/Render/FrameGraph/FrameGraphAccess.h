#pragma once

#include "FrameGraphHandles.h"
#include "FrameGraphResources.h"

#include "Candle/Core/Asserts.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <limits>

namespace Candle {

	enum class FGResourceType : uint8_t { Texture, Buffer };

	struct FGAccessEntry
	{
		FGResourceType Type{ FGResourceType::Texture };
		uint32_t ResourceIndex{ std::numeric_limits<uint32_t>::max() };	// Into m_Textures or m_Buffers, by Type
		uint32_t Version{ 0 };
		FGUsage Usage;
		bool Write{ false };				// If true, the pass writes to the resource, so it needs a barrier
		FGLoad LoadOp{ FGLoad::DontCare };	// Attachments only
		vk::ClearValue ClearValue;			// Attachments only
		uint32_t SrcPassIndex{ std::numeric_limits<uint32_t>::max() };	// Set by the builder during setup: the pass that wrote what this access reads. None for a Clear or DontCare.

		inline FGTextureHandle GetTextureHandle() const
		{
			CDL_CORE_ASSERT(Type == FGResourceType::Texture, "Access is not to a texture");
			return { .Index = ResourceIndex, .Version = Version };
		}
	};

}
