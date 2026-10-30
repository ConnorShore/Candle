#pragma once

#include "Candle/Core/Core.h"
#include "Candle/App/ApplicationSpecification.h"
#include "Candle/Core/Time.h"
#include "Candle/Platform/Window.h"
#include "Candle/World/World.h"

#include "Candle/Asset/AssetManager.h"
#include "Candle/Input/InputSystem.h"
#include "Candle/Core/Job/JobSystem.h"

#include <atomic>

namespace Candle {

	class RenderManager;
	struct FramePacket;

	struct ApplicationContext
	{
		Candle::AssetManager& AssetManager;
		World& ActiveWorld;
	};

	struct UpdateContext
	{
		ApplicationContext& AppContext;
		TimeStep DeltaTime;
		const InputSnapshot& Input;
	};

    class Application
    {
    public:
        Application(const ApplicationSpecification& appSpecs);
		virtual ~Application();

		virtual void OnInit(ApplicationContext context) = 0;
		virtual void OnUpdate(UpdateContext context) = 0;
		virtual void OnShutdown() = 0;

        void Run();

		inline void RequestQuit() { m_QuitRequested.store(true, std::memory_order_relaxed); }

		inline const ApplicationSpecification& GetSpecification() const { return m_Specification; }	// Asset directories already absolute

	private:
		void HandlePlatformEvents();
		void ExtractToFramePacket(FramePacket& packet);

		inline bool IsQuitRequested() const { return m_QuitRequested.load(std::memory_order_relaxed); }

	private:
		struct FrameStats
		{
			uint64_t FrameCount = 0;
			TimeStep DeltaTime = 0.0f;
			double TotalTime = 0.0f;

			// Accumulated since the last FPS log, then reset
			double TimeSinceLastLog = 0.0f;
			uint32_t FramesSinceLastLog = 0;
			float WorstFrameTime = 0.0f;	// Milliseconds
		};

		struct PlatformScope { PlatformScope(); ~PlatformScope(); };
		struct LoggerScope { explicit LoggerScope(const LoggerSpecification& spec); ~LoggerScope(); };
		struct WindowingScope { explicit WindowingScope(bool enabled); ~WindowingScope(); bool Enabled; };

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
		AssetManager m_AssetManager;

		World m_ActiveWorld;						// The world that the application is currently simulating and rendering (TEMPORARY: one world only for now)

		std::atomic<bool> m_QuitRequested{ false };
		FrameStats m_FrameStats;

		std::vector<PlatformEvent> m_FrameEvents;
	};

	ScopedPtr<Application> CreateApplication(int argc, char** argv);
}