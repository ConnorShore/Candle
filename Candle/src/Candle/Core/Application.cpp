#include "cdlpch.h"
#include "Application.h"

#include "Candle/Platform/Platform.h"

namespace Candle {

	Application::Application(const ApplicationSpecification& appSpecs)
		: m_Specification(appSpecs)
	{
		Platform::Init();
		Logger::Init(m_Specification.LoggerSpec);

		CDL_CORE_INFO(LogChannel::Application, "Application created: {}", m_Specification.Name);
	}

	Application::~Application()
	{
		CDL_CORE_INFO(LogChannel::Application, "Application destroyed: {}", m_Specification.Name);

		Logger::Shutdown();
	}

	void Application::Run()
	{

		while (!IsQuitRequested())
		{
			Tick startTick = Platform::GetTick();

			// TODO: Implement game logic

			m_DeltaTime = TimeStep(Platform::ToSeconds(startTick, Platform::GetTick()));
			CDL_CORE_INFO(LogChannel::Application, "Application running: {}; Frame FPS: {}", m_Specification.Name, (1.0f / m_DeltaTime));
		}
	}

}
