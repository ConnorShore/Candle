#pragma once

#ifdef CANDLE_PLATFORM_WINDOWS

extern Candle::Application* Candle::CreateApplication();

int main(int argc, char** argv)
{
	Candle::Log::Init();
	CANDLE_CORE_WARN("Initialized Log!");
	int a = 5;
	CANDLE_INFO("Initialized Log! {0}", a);

	auto app = Candle::CreateApplication();
	app->Run();
	delete app;
}

#endif