#pragma once

#include "Candle/Core.h"
#include "Candle/Renderer/RendererAPI.h"

namespace Candle {

	class CANDLE_API RenderCommand
	{
	public:

		inline static void SetClearColor(const glm::vec4& color)
		{
			_rendererAPI->SetClearColor(color);
		}

		inline static void Clear()
		{
			_rendererAPI->Clear();
		}

		inline static void DrawIndexed(const Ref<VertexArray>& vertexArray)
		{
			_rendererAPI->DrawIndexed(vertexArray);
		}
		
	private:
		static RendererAPI* _rendererAPI;
	};

}