#include "cdlpch.h"
#include "RenderManager.h"
#include "ShaderLoader.h"
#include "Shader.h"

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

	RenderManager::RenderManager(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo, Window& window) 
		: m_Specification(renderSpec)
		, m_Window(window)
		, m_RenderInstance(CreateRenderInstanceSpec(renderSpec, appInfo))
		, m_RenderDevice(m_RenderInstance, m_Specification)
	{
		// Load a test shader
		auto shader = ShaderLoader::LoadShader("Tester", "C:\\Development\\Projects\\Candle\\Candle\\res\\shaders\\bin\\Test.spv", m_RenderDevice);
		CDL_CORE_INFO(LogChannel::Render, "Shader loaded: {}", shader.GetName());
	}

	RenderManager::~RenderManager()
	{
	}

}