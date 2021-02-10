#include <Candle.h>

class ExampleLayer : public Candle::Layer
{
public:
	ExampleLayer() : Layer("Example") {}

	void OnUpdate() override
	{
		CANDLE_INFO("ExampleLayer::Update");
	}

	void OnEvent(Candle::Event& e) override
	{
		CANDLE_TRACE("{0}", e);
	}
};

class SandboxApp : public Candle::Application
{
public:
	SandboxApp()
	{
		PushLayer(new ExampleLayer());
		PushOverlay(new Candle::ImGuiLayer());
	}

	~SandboxApp()
	{
	}
};

Candle::Application* Candle::CreateApplication()
{
	return new SandboxApp();
}