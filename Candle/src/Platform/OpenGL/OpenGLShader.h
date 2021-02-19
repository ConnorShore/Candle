#pragma once

#include "Candle/Renderer/Shader.h"

namespace Candle {

	class OpenGLShader : public Shader
	{
	public:
		OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc);
		~OpenGLShader();

		void Bind() const;
		void Unbind() const;

	private:
		uint32_t _rendererId;
	};
}