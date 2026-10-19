#include "cdlpch.h"
#include "TrianglePipeline.h"

#include "Candle/Render/RenderDevice.h"
#include "Candle/Render/ShaderLoader.h"


namespace Candle {

	PipelineSpecification TrianglePipeline::CreateSpecification(RenderDevice& renderDevice, vk::Format format) const
	{
		// Create triangle pipeline here for now, will eventually be created on demand when a pipeline is needed
		SharedPtr<Shader> shader = ShaderLoader::LoadShader(
			m_Name,
			"C:/Development/Projects/Candle/Candle/res/shaders/bin/Triangle.spv",
			renderDevice
		);

		return {
			.Shader = shader,
			.ColorAttachmentFormats = {format}
			// Rest are defaults for now
		};
	}

}