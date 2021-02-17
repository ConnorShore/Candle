#pragma once

#include "Candle/Core.h"

namespace Candle {

	enum class RendererAPI
	{
		None = 0,
		OpenGL = 1,
		DirectX = 2,
		Vulkan = 3,
		Metal = 4
	};

	class CANDLE_API Renderer
	{
	public:
		inline static RendererAPI GetAPI() { return _rendererAPI; }

	private:
		static RendererAPI _rendererAPI;

	};
}