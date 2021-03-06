#pragma once

#include "Candle/Renderer/RendererAPI.h"

namespace Candle {

	class OpenGLRendererAPI : public RendererAPI
	{
	public:

		OpenGLRendererAPI();
		~OpenGLRendererAPI();

		virtual void Init() override;

		virtual void Clear() override;
		virtual void SetClearColor(const glm::vec4& color) override;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) override;
	};
}