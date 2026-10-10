#pragma once

#include "Core.h"
#include "ApplicationSpecification.h"
#include "Time.h"
#include "Window.h"

#include "Candle/Core/Input/InputSystem.h"
#include "Candle/Core/Job/JobSystem.h"

#include <atomic>

namespace Candle {

	class RenderManager;

    class Application
    {
    public:
        Application(const ApplicationSpecification& appSpecs);
		virtual ~Application();

		virtual void OnInit() = 0;
		virtual void OnShutdown() = 0;

        void Run();

		inline void RequestQuit() { m_QuitRequested.store(true, std::memory_order_relaxed); }

	private:
		void HandlePlatformEvents();

		inline bool IsQuitRequested() const { return m_QuitRequested.load(std::memory_order_relaxed); }

	private:
		struct FrameStats
		{
			uint64_t FrameCount = 0;
			TimeStep DeltaTime = 0.0f;
			double TotalTime = 0.0f;
		};

		struct PlatformScope { PlatformScope(); ~PlatformScope(); };
		struct LoggerScope { explicit LoggerScope(const LoggerSpecification& spec); ~LoggerScope(); };
		struct WindowingScope { explicit WindowingScope(bool enabled); ~WindowingScope(); bool m_Enabled; };

    private:
		// Declared in startup order so that they are destroyed in reverse order
		ApplicationSpecification m_Specification;
		PlatformScope m_PlatformScope;
		LoggerScope m_LoggerScope;
		WindowingScope m_WindowingScope;			// After the logger, so a backend failure can be logged
		ScopedPtr<Window> m_Window;					// Empty when headless
		InputSystem m_InputSystem;					// Never fed when headless, since nothing is pumped
		JobSystem m_JobSystem;
		ScopedPtr<RenderManager> m_RenderManager;	// Empty when headless

		std::atomic<bool> m_QuitRequested{ false };
		FrameStats m_FrameStats;

		std::vector<PlatformEvent> m_FrameEvents;
	};

	ScopedPtr<Application> CreateApplication(int argc, char** argv);
}