#pragma once

#include "Core.h"
#include "ApplicationSpecification.h"
#include "Time.h"

#include <atomic>

namespace Candle {

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
		inline bool IsQuitRequested() const { return m_QuitRequested.load(std::memory_order_relaxed); }

	private:
		struct FrameStats
		{
			uint64_t FrameCount = 0;
			TimeStep DeltaTime = 0.0f;
			double TotalTime = 0.0f;
		};

    private:
		ApplicationSpecification m_Specification;	// TODO: May make this a PlatformSpecification in the future (or pass parts onto a platform specification)

		std::atomic<bool> m_QuitRequested{ false };
		FrameStats m_FrameStats;
	};

	ScopedPtr<Application> CreateApplication(int argc, char** argv);
}