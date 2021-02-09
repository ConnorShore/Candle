#pragma once

#include "Core.h"
#include "Events/Event.h"
#include "Candle/Events/ApplicationEvent.h"
#include "Window.h"

namespace Candle {

	class CANDLE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void OnEvent(Event& e);
		void Run();
	private:
		bool OnWindowClose(WindowCloseEvent& e);

	private:
		std::unique_ptr<Window> _window;
		bool _isRunning = true;
	};

	// To be defined in the client
	Application* CreateApplication();
}
