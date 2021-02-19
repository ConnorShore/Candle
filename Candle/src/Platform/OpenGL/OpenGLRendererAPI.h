#pragma once

#include "Candle/Renderer/RendererAPI.h"

namespace Candle {

	class OpenGLRendererAPI : public RendererAPI
	{
	public:

		OpenGLRendererAPI();
		~OpenGLRendererAPI();

		virtual void Clear() override;
		virtual void SetClearColor(const glm::vec4& color) override;

		virtual void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray) override;
	};
}