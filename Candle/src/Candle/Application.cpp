#include "candlepch.h"

#include "Application.h"
#include "Events/Event.h"
#include "Candle/Events/ApplicationEvent.h"

namespace Candle {

	Application::Application()
	{
		_window = std::unique_ptr<Window>(Window::Create());
	}

	Application::~Application()
	{
	}

	void Application::Run()
	{
		while (_isRunning) {
			_window->OnUpdate();
		}
	}
}