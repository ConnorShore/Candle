#pragma once

#include "Core.h"
#include "ApplicationSpecification.h"

namespace Candle {

    class Application
    {
    public:
        Application(const ApplicationSpecification& appSpecs);
		virtual ~Application();

		virtual void OnInit() = 0;
		virtual void OnShutdown() = 0;

        void Run();

    private:
		ApplicationSpecification m_Specification;	// TODO: May make this a PlatformSpecification in the future (or pass parts onto a platform specification)
	};

	ScopedPtr<Application> CreateApplication(int argc, char** argv);
}