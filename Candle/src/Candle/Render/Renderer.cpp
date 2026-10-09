#include "cdlpch.h"
#include "Renderer.h"
#include "RenderDevice.h"
#include "PipelineSpecification.h"
#include "ShaderLoader.h"

namespace Candle {

	Renderer::Renderer(RenderDevice& renderDevice, vk::Format targetFormat)
		: m_RenderDevice(renderDevice), m_TargetFormat(targetFormat)
	{
		// Create triangle pipeline here for now, will eventually be created on demand when a pipeline is needed
		SharedPtr<Shader> shader = ShaderLoader::LoadShader(
			"BasicTriangle", 
			"C:/Development/Projects/Candle/Candle/res/shaders/bin/Triangle.spv",
			m_RenderDevice
		);
		PipelineSpecification spec{
			.Shader = shader
			// Rest are defaults for now
		};

		CreatePipeline(spec);
	}

	void Renderer::CreatePipeline(PipelineSpecification& spec)
	{
		m_Pipelines.emplace_back(spec, m_RenderDevice, m_TargetFormat);
	}

}