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
		Logger::Shutdown();

		CDL_CORE_INFO(LogChannel::Application, "Application destroyed: {}", m_Specification.Name);
	}

	void Application::Run()
	{
		CDL_CORE_INFO(LogChannel::Application, "Application running: {}", m_Specification.Name);
	}

}
