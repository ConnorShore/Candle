#include "cdlpch.h"
#include "RenderManager.h"

#include "Candle/Core/Window.h"

namespace Candle {

	RenderManager::RenderManager(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo, Window& window)
		: m_Specification(renderSpec), m_Window(window)
	{
		CreateRenderInstance(appInfo);
	}

	RenderManager::~RenderManager()
	{
		DestroyRenderInstance();
	}

	void RenderManager::CreateRenderInstance(const ApplicationInfo& appInfo)
	{
		RenderInstanceSpecification instanceSpec = {
			.ApplicationName = appInfo.ApplicationName
			.EngineName = appInfo.EngineName
		};
	}

	void RenderManager::DestroyRenderInstance()
	{

	}

}