#pragma once

#include "Core.h"
#include "Window.h"

namespace Candle {

	class CANDLE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

	private:
		std::unique_ptr<Window> _window;
		bool _isRunning = true;
	};

	// To be defined in the client
	Application* CreateApplication();
}
