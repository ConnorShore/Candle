#include "cdlpch.h"
#include "Application.h"

namespace Candle {

	Application::Application(const ApplicationSpecification& appSpecs)
		: m_Specification(appSpecs)
	{
		std::cout << "Application created: " << m_Specification.Name << std::endl;

		//m_Platform = Platform::Create(); <- Once scoped ptr implementation is done
	}

	Application::~Application()
	{
		std::cout << "Application destroyed: " << m_Specification.Name << std::endl;
	}

	void Application::Run()
	{
		std::cout << "Application running: " << m_Specification.Name << std::endl;
	}

}
