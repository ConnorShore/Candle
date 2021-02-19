#include "candlepch.h"
#include "RenderCommand.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Candle {

	RendererAPI* RenderCommand::_rendererAPI = new OpenGLRendererAPI;


}