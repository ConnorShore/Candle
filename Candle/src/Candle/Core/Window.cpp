#include "cdlpch.h"
#include "Window.h"


namespace Candle {

	Window::Window(const WindowSpecification& info)
		: m_Title(info.Title), m_Width(info.Width), m_Height(info.Height)
	{
		m_NativeHandle = Platform::CreateWindow(info);
	}

	Window::~Window()
	{
		Close();
	}

	void Window::Resize(uint32_t width, uint32_t height)
	{
		Platform::ResizeWindow(m_NativeHandle, width, height);
	}

	void Window::Minimize()
	{
		Platform::MinimizeWindow(m_NativeHandle);
	}

	void Window::Maximize()
	{
		Platform::MaximizeWindow(m_NativeHandle);
	}

	void Window::Restore()
	{
		Platform::RestoreWindow(m_NativeHandle);
	}

	void Window::Close()
	{
		if (m_NativeHandle != 0)
		{
			Platform::DestroyWindow(m_NativeHandle);
			m_NativeHandle = 0;
		}
	}
}