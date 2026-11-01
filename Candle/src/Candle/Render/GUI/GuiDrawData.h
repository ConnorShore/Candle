#pragma once

#include "Candle/Render/Resources/GPUHandles.h"

#include <glm/glm.hpp>

#include <vector>

namespace Candle {

	struct GuiVertex
	{
		glm::vec2 Position;
		glm::vec2 UV;
		uint32_t Color;	// RGBA8 packed into a uint32_t
	};
	using GuiIndex = uint16_t;	

	struct GuiDrawCmd
	{
		glm::vec4 ClipRect;             // minX, minY, maxX, maxY in framebuffer pixels: DisplayPos removed and FramebufferScale applied at extraction
		GPUTextureHandle Texture;
		uint32_t VertexOffset;
		uint32_t IndexOffset;
		uint32_t IndexCount;
	};

	struct GuiDrawData
	{
		std::vector<GuiVertex> Vertices;
		std::vector<GuiIndex> Indices;
		std::vector<GuiDrawCmd> Commands;
		glm::vec2 DisplayPos;
		glm::vec2 DisplaySize;
	};

}