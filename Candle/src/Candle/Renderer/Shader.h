#pragma once

#include "Candle/Core.h"
#include <string>

namespace Candle {
	
	class CANDLE_API Shader
	{
	public:
		Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
		~Shader();

		void Bind() const;
		void Unbind() const;

	private:
		uint32_t _rendererId;
	};
}