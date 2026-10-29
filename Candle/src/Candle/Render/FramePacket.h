#pragma once

#include "GPUHandles.h"
#include "SamplerType.h"

#include <vector>
#include <glm/glm.hpp>

namespace Candle {

	struct DrawItem
	{
		GPUMeshHandle Mesh;
		GPUTextureHandle Material;
		SamplerType Sampler;
		uint32_t TransformIndex;	// Index into the FramePacket's TransformData array
	};

	// A packet of data that is sent to the renderer for rendering. This will be filled by the simulation and then sent to the render thread for rendering.
	// Threading: simulation thread fills it, render thread reads it. The simulation thread must not modify the packet after it has been sent to the render thread.
	struct FramePacket
	{
		// Vector not span as data is copied to the render thread, and the simulation thread may modify its own copy of the vector after sending it to the render thread
		std::vector<DrawItem> DrawItems;
		std::vector<glm::mat4> TransformData;	// Model matrices for each draw item; the renderer writes them into a GPU buffer and shaders read them by address
		glm::mat4 ViewMatrix{ 1.0f };
		glm::mat4 ProjectionMatrix{ 1.0f };
		double Time{ 0.0 };		// Simulation time in seconds at extraction; the renderer never reads a clock of its own

		bool Minimized{ false };		// Whether the surface is minimized; the renderer may skip rendering if true
		glm::uvec2 SurfaceSize{ 0, 0 };	// The  size of the surface for this frame; the renderer may need to recreate the swapchain if changed

		std::vector<GPUSlotRef> ReleasedSlots;	// Released by the main thread; no later packet may reference them
		std::vector<GPUSlotRef> RecycledSlots;	// The one field the render thread writes: freed slots, read when the packet is next acquired for writing

		uint64_t FrameIndex{ 0 };	// The index of the frame for this packet; used for debugging and profiling
	};

}