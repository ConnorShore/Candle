#pragma once

#include "Candle/Core/Core.h"
#include "Candle/App/Application.h"

#include <exception>
#include <iostream>

#ifdef CDL_PLATFORM_WINDOWS

extern Candle::ScopedPtr<Candle::Application> Candle::CreateApplication(int argc, char** argv);

int main(int argc, char** argv)
{
	try
	{
		auto app = Candle::CreateApplication(argc, argv);
		app->Run();
		return 0;
	}
	catch (const std::exception& e)
	{
		// The application, and its logger with it, has already been destroyed by the time this runs
		std::cout << "Unhandled exception in application: " << e.what() << std::endl;
		return -1;
	}
}

#else
#error Only Windows is supported!
#endif