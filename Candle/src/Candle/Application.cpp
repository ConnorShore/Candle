#include "candlepch.h"

#include "Application.h"
#include "Candle/Events/Event.h"
#include "Candle/Events/ApplicationEvent.h"
#include "Candle/Log.h"

namespace Candle {

	Application::Application()
	{

	}

	Application::~Application()
	{

	}

	void Application::Run()
	{
		WindowResizeEvent e(1280, 720);
		CANDLE_TRACE(e);

		while (true);
	}
}