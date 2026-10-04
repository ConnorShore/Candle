#include "cdlpch.h"
#include "Application.h"

#include "Candle/Platform/Platform.h"

namespace Candle {

	Application::Application(const ApplicationSpecification& appSpecs)
		: m_Specification(appSpecs)
	{
		Platform::Init();
		Logger::Init(m_Specification.LoggerSpec);

		m_Window = ScopedPtr<Window>::Create(appSpecs.WindowSpec);

		CDL_CORE_INFO(LogChannel::Application, "Application created: {}", m_Specification.Name);
	}

	Application::~Application()
	{
		CDL_CORE_INFO(LogChannel::Application, "Application destroyed: {}", m_Specification.Name);

		Logger::Shutdown();
		Platform::Shutdown();
	}

	void Application::Run()
	{
		double timeSinceLastLog = 0.0f;	// Temporary variable to track time since last log message
		Tick startTick = Platform::GetTick();
		while (!IsQuitRequested())
		{
			// Process events
			std::vector<PlatformEvent> events;
			Platform::PumpEvents(events);
			for (auto& evt : events)
			{
				if (std::holds_alternative<QuitRequested>(evt))
				{
					RequestQuit();
				}
			}

			// TODO: Implement game logic

			// Update the frame stats
			Tick endTick = Platform::GetTick();
			m_FrameStats.DeltaTime = TimeStep(Platform::ToSeconds(startTick, endTick));
			m_FrameStats.TotalTime += m_FrameStats.DeltaTime;
			m_FrameStats.FrameCount++;
			startTick = endTick;

			// Display log every 1/2 second
			timeSinceLastLog += m_FrameStats.DeltaTime;
			if (timeSinceLastLog >= 0.5f)
			{
				CDL_CORE_INFO(LogChannel::Application, "Application running: {}; Frame FPS: {}", m_Specification.Name, (1.0f / m_FrameStats.DeltaTime));
				timeSinceLastLog = 0.0f;
			}
		}
	}

}
