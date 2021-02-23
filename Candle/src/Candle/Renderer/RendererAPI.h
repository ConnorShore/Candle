#pragma once

#include "Candle/Core.h"
#include "Candle/Renderer/VertexArray.h"

#include <glm/glm.hpp>

namespace Candle {

	class CANDLE_API RendererAPI
	{
	public:
		enum class API
		{
			None = 0,
			OpenGL = 1,
			DirectX = 2,
			Vulkan = 3,
			Metal = 4
		};

	public:
		virtual ~RendererAPI() { }
	
		virtual void Clear() = 0;
		virtual void SetClearColor(const glm::vec4& color) = 0;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) = 0;

		static inline API GetAPI() { return _API; }
		
	private:
		static API _API;
	};

}