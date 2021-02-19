#pragma once

#include "Candle/Renderer/RenderCommand.h"

namespace Candle {

	class CANDLE_API Renderer
	{
	public:

		static void BeginScene();	// TODO: Scene parameters
		static void EndScene();

		static void Submit(const std::shared_ptr<VertexArray>& vertexArray);

		inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	};
}