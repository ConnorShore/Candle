#include "candlepch.h"
#include "WindowsWindow.h"

#include "Candle/Events/ApplicationEvent.h"
#include "Candle/Events/KeyEvent.h"
#include "Candle/Events/MouseEvent.h"

#include <glad/glad.h>

namespace Candle {

	static bool s_glfwInitialized = false; // Make static as we may have multiple windows, but only want glwf initialized once

	static void GLFWErrorCallback(int error, const char* description)
	{
		CANDLE_CORE_ERROR("GLFW Error [{0}]: {1}", error, description);
	}

	Window* Window::Create(const WindowProps& props)
	{
		return new WindowsWindow(props);
	}

	WindowsWindow::WindowsWindow(const WindowProps& props)
	{
		Init(props);
	}

	WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	void WindowsWindow::Init(const WindowProps& props)
	{
		_data.Title = props.Title;
		_data.Width = props.Width;
		_data.Height = props.Height;

		CANDLE_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);

		if (!s_glfwInitialized) 
		{
			int success = glfwInit();
			CANDLE_CORE_ASSERT(success, "Could not initalize GLFW");
			glfwSetErrorCallback(GLFWErrorCallback);
			s_glfwInitialized = true;
		}

		_window = glfwCreateWindow((int)props.Width, (int)props.Height, _data.Title.c_str(), nullptr, nullptr);
		glfwMakeContextCurrent(_window);

		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		CANDLE_CORE_ASSERT(status, "Failed to initialize Glad!");

		glfwSetWindowUserPointer(_window, &_data);
		SetVSync(true);

		// Set GLFW Callbacks
		glfwSetWindowSizeCallback(_window, [](GLFWwindow* window, int width, int height)
			{
				WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
				data.Width = width;
				data.Height = height;

				WindowResizeEvent evt(width, height);
				data.EventCallback(evt);
			});

		glfwSetWindowCloseCallback(_window, [](GLFWwindow* window)
			{
				WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
				WindowCloseEvent evt;
				data.EventCallback(evt);
			});

		glfwSetKeyCallback(_window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
			{
				// Need to convert glfw key code (key) to candle key code
				WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

				switch (action)
				{
					case GLFW_PRESS:
					{
						KeyPressedEvent evt(key, 0);
						data.EventCallback(evt);
						break;
					}
					case GLFW_RELEASE:
					{
						KeyReleasedEvent evt(key);
						data.EventCallback(evt);
						break;
					}
					case GLFW_REPEAT:
					{
						KeyPressedEvent evt(key, 1);
						data.EventCallback(evt);
						break;
					}
				}
			});

		glfwSetCharCallback(_window, [](GLFWwindow* window, unsigned int keycode)
			{
				WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
				KeyTypedEvent evt(keycode);
				data.EventCallback(evt);
			});

		glfwSetMouseButtonCallback(_window, [](GLFWwindow* window, int button, int action, int mods)
			{
				WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

				switch (action)
				{
					case GLFW_PRESS:
					{
						MouseButtonPressedEvent evt(button);
						data.EventCallback(evt);
						break;
					}
					case GLFW_RELEASE:
					{
						MouseButtonReleasedEvent evt(button);
						data.EventCallback(evt);
						break;
					}
				}
			});

		glfwSetScrollCallback(_window, [](GLFWwindow* window, double xOffset, double yOffset)
			{
				WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
				
				MouseScrolledEvent evt((float)xOffset, (float)yOffset);
				data.EventCallback(evt);
			});

		glfwSetCursorPosCallback(_window, [](GLFWwindow* window, double xPos, double yPos)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseMovedEvent evt((float)xPos, (float)yPos);
			data.EventCallback(evt);
		});
	}

	void WindowsWindow::OnUpdate()
	{
		glfwPollEvents();
		glfwSwapBuffers(_window);
	}

	void WindowsWindow::Shutdown()
	{
		glfwDestroyWindow(_window);
	}

	void WindowsWindow::SetVSync(bool enabled)
	{
		if (enabled)
			glfwSwapInterval(1);
		else
			glfwSwapInterval(0);

		_data.VSync = enabled;
	}

	bool WindowsWindow::IsVSync() const
	{
		return _data.VSync;
	}
}
