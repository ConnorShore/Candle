#pragma once

#include "Core.h"
#include "Application.h"

#include <exception>
#include <iostream>

#ifdef CDL_PLATFORM_WINDOWS

extern Candle::ScopedPtr<Candle::Application> Candle::CreateApplication(int argc, char** argv);

int main(int argc, char** argv)
{
	try
	{
		auto app = Candle::CreateApplication(argc, argv);
		app->OnInit();
		app->Run();
		app->OnShutdown();
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cout << "Unhandled exception while starting application: " << e.what() << std::endl;
		return -1;
	}
}

#else
#error Only Windows is supported!
#endif