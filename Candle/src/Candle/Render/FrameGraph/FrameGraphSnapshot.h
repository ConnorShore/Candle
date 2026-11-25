#pragma once

#include "FrameGraphAccess.h"
#include "FrameGraphCompilerResources.h"

#include "Candle/Render/Resources/GPUHandles.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace Candle {

	// The name FrameGraphPreviewModule gives its pass, so the snapshot shows when the preview target was drawn
	inline constexpr std::string_view kFrameGraphPreviewPassName = "FrameGraphPreview";

	// What the render graph viewer asks of one packet. Filled by FrameGraphCapture; the render thread only reads it.
	struct FrameGraphCaptureRequest
	{
		bool Snapshot{ false };
		std::string PreviewTexture;				// A transient's name. Keeps a preview target for it; empty releases the target
		bool CapturePreview{ false };			// Draw into the target this frame; off while paused, so the preview holds still
		glm::vec2 PreviewRange{ 0.0f, 1.0f };	// Remapped to black..white, so depth crowded near 1 can be spread out
		GPUTextureHandle PreviewTarget;			// Set by FrameGraphCapture; invalid unless CapturePreview
	};

	// A copy of one frame's compiled graph that owns everything it names, so it outlives the frame and crosses threads in the packet.
	// Textures mirror the graph's one to one, so the texture indices in Accesses and Barriers index Textures here.
	struct FrameGraphSnapshot
	{
		static constexpr uint32_t kNone = std::numeric_limits<uint32_t>::max();

		struct Pass
		{
			std::string Name;
			bool Compute{ false };
			bool NeverCull{ false };
			bool Live{ false };
			uint32_t AccessStart{ 0 }, AccessCount{ 0 };
			uint32_t BarrierStart{ 0 }, BarrierCount{ 0 };	// Live passes only
		};

		struct Texture
		{
			std::string Name;
			bool Imported{ false };
			glm::uvec2 Extent{ 0, 0 };
			vk::Format Format{ vk::Format::eUndefined };
			vk::ImageUsageFlags Usage{};	// A transient's comes from live passes only, so it's empty when none of them uses it
			uint32_t LatestVersion{ 0 };	// How many times it was written
			uint32_t CacheIndex{ kNone };	// Transients only
			uint32_t BindlessIndex{ kNone };
		};

		std::vector<Pass> Passes;					// In declaration order, culled ones included
		std::vector<Texture> Textures;
		std::vector<FGAccessEntry> Accesses;		// Each pass's in one run, as in the graph
		std::vector<FGBarrier> Barriers;			// Each live pass's in one run, in execution order
		std::vector<FGBarrier> FinalBarriers;		// The imports, back to where the next frame expects them
		glm::uvec2 OutputExtent{ 0, 0 };
		uint64_t FrameIndex{ 0 };
		bool Captured{ false };						// False when the render thread drew nothing for the packet, e.g. while minimized
	};

}
