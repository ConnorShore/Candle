#include "cdlpch.h"
#include "Application.h"

#include "Candle/Render/RenderManager.h"

#include "Candle/Platform/Platform.h"

namespace Candle {

	Application::PlatformScope::PlatformScope() { Platform::Init(); }
	Application::PlatformScope::~PlatformScope() { Platform::Shutdown(); }

	Application::LoggerScope::LoggerScope(const LoggerSpecification& spec) { Logger::Init(spec); }
	Application::LoggerScope::~LoggerScope() { Logger::Shutdown(); }

	Application::WindowingScope::WindowingScope(bool enabled) : m_Enabled(enabled) { if (m_Enabled) Platform::InitWindowing(); }
	Application::WindowingScope::~WindowingScope() { if (m_Enabled) Platform::ShutdownWindowing(); }

	Application::Application(const ApplicationSpecification& appSpecs)
		: m_Specification(appSpecs)
		, m_LoggerScope(m_Specification.LoggerSpec)
		, m_WindowingScope(!m_Specification.Headless)
		, m_Window(m_Specification.Headless ? ScopedPtr<Window>() : ScopedPtr<Window>::Create(m_Specification.WindowSpec))
		, m_RenderManager(m_Specification.Headless ? ScopedPtr<RenderManager>() : ScopedPtr<RenderManager>::Create(m_Specification.RenderSpec, m_Specification.AppInfo, *m_Window))
	{
		CDL_CORE_INFO(LogChannel::Application, "Application created: {}", m_Specification.AppInfo.Name);
	}

	Application::~Application()
	{
		CDL_CORE_INFO(LogChannel::Application, "Application destroyed: {}", m_Specification.AppInfo.Name);
	}

	void Application::Run()
	{
		double timeSinceLastLog = 0.0f;	// Temporary variable to track time since last log message
		Tick startTick = Platform::GetTick();
		while (!IsQuitRequested())
		{
			// Process events
			HandlePlatformEvents();

			// Moves inside the fixed-step tick loop once there is one, so each tick consumes the edges exactly once
			InputSnapshot inputSnapshot = m_InputSystem.CaptureSnapshot();

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
				CDL_CORE_INFO(LogChannel::Application, "Application running: {}; Frame FPS: {}", 
					m_Specification.AppInfo.Name, (1.0f / m_FrameStats.DeltaTime));
				CDL_CORE_INFO(LogChannel::Input, "Space bar Pressed: {}; Left mouse button pressed: {}", 
					inputSnapshot.IsKeyDown(KeyCode::Space), inputSnapshot.IsMouseDown(MouseButton::Left));

				timeSinceLastLog = 0.0f;
			}
		}
	}

	void Application::HandlePlatformEvents()
	{
		// If headless, we don't have a window to receive events from, so we skip event handling
		if (m_Specification.Headless)
			return;

		Platform::PumpEvents(m_FrameEvents);
		for (auto& evt : m_FrameEvents)
		{
			std::visit(Overloaded{
				[&](const QuitRequested&) { RequestQuit(); },
				[&](const WindowCloseRequested& e) { if (e.WindowID == m_Window->GetID()) RequestQuit(); },
				[&](const WindowResized& e) {},
				[&](const WindowFocus& e) {},
				[&](const WindowMinimized& e) {},
				[&](const InputEvent& e) { m_InputSystem.ConsumeEvent(e); }
				}, evt);
		}
	}

}
