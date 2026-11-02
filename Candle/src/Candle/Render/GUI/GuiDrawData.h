#pragma once

#include "GUIVertex.h"
#include "Candle/Render/Resources/GPUHandles.h"

#include <glm/glm.hpp>

#include <vector>

namespace Candle {

	using GuiIndex = uint16_t;	

	struct GuiDrawCommand
	{
		glm::vec4 ClipRect;             // minX, minY, maxX, maxY in framebuffer pixels: DisplayPos removed and FramebufferScale applied at extraction
		GPUTextureHandle Texture;
		uint32_t VertexOffset;
		uint32_t IndexOffset;
		uint32_t IndexCount;
	};

	struct GuiDrawData
	{
		std::vector<Candle::GPU::GuiVertex> Vertices;
		std::vector<GuiIndex> Indices;
		std::vector<GuiDrawCommand> Commands;
		glm::vec2 DisplayPos;
		glm::vec2 DisplaySize;
	};

}