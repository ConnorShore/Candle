#include "cdlpch.h"
#include "RenderManager.h"

#include "Candle/Core/Window.h"

namespace Candle {

	namespace {

		RenderInstanceSpecification CreateRenderInstanceSpec(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo)
		{
			return {
				.VulkanVersion = renderSpec.VulkanVersion,
				.ValidationSpec = renderSpec.ValidationSpec,
				.ApplicationName = appInfo.Name,
				.ApplicationVersion = appInfo.Version
			};
		}

	}

	RenderManager::RenderManager(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo, Window& window) :
		m_Specification(renderSpec),
		m_Window(window),
		m_RenderInstance(CreateRenderInstanceSpec(renderSpec, appInfo))
	{
	}

	RenderManager::~RenderManager()
	{
	}

}