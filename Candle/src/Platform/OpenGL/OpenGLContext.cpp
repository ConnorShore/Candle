#include "candlepch.h"
#include "OpenGLContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace Candle {

	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle) : _windowHandle(windowHandle)
	{
		CANDLE_CORE_ASSERT(_windowHandle, "Window handle is null!");
	}

	void OpenGLContext::Init()
	{
		glfwMakeContextCurrent(_windowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		CANDLE_CORE_ASSERT(status, "Failed to initialize Glad!");

		CANDLE_CORE_INFO("OpenGL Info:");
		CANDLE_CORE_INFO("  Vendor: {0}", glGetString(GL_VENDOR));
		CANDLE_CORE_INFO("  Renderer: {0}", glGetString(GL_RENDERER));
		CANDLE_CORE_INFO("  Version: {0}", glGetString(GL_VERSION));
	}

	void OpenGLContext::SwapBuffer()
	{
		glfwSwapBuffers(_windowHandle);
	}
}
