#include "candlepch.h"
#include "Application.h"
#include "Input.h"

#include <glad/glad.h>

namespace Candle {

	Application* Application::s_instance = nullptr;

	Application::Application()
	{
		CANDLE_CORE_ASSERT(!s_instance, "Application already exists!");
		s_instance = this;

		_window = std::unique_ptr<Window>(Window::Create());
		_window->SetEventCallback(CANDLE_BIND_EVENT_FN(Application::OnEvent));
	}

	Application::~Application()
	{
	}

	void Application::PushLayer(Layer* layer)
	{
		_layerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* overlay)
	{
		_layerStack.PushOverlay(overlay);
		overlay->OnAttach();
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(CANDLE_BIND_EVENT_FN(Application::OnWindowClose));

		for (auto it = _layerStack.end(); it != _layerStack.begin(); ) 
		{
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}

	void Application::Run()
	{
		while (_isRunning) 
		{
			glClearColor(0, 0, 1, 1);
			glClear(GL_COLOR_BUFFER_BIT);

			for (Layer* layer : _layerStack)
				layer->OnUpdate();

			_window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		_isRunning = false;
		return true;
	}
}