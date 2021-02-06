#include <Candle.h>

class SandboxApp : public Candle::Application
{
public:
	SandboxApp()
	{

	}

	~SandboxApp()
	{

	}
};

Candle::Application* Candle::CreateApplication()
{
	return new SandboxApp();
}