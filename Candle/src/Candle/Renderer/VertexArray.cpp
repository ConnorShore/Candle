#include "candlepch.h"
#include "VertexArray.h"

#include "Candle/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Candle {

	VertexArray* VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::None:
			CANDLE_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
			return nullptr;
		case RendererAPI::OpenGL:
			return new OpenGLVertexArray();
		}

		CANDLE_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}