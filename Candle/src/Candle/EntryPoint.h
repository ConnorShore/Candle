#pragma once

#ifdef CANDLE_PLATFORM_WINDOWS

extern Candle::Application* Candle::CreateApplication();

int main(int argc, char** argv)
{
	auto app = Candle::CreateApplication();
	app->Run();
	delete app;
}

#endif