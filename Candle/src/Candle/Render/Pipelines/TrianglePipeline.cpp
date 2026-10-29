#include "cdlpch.h"
#include "TrianglePipeline.h"

#include "Candle/Render/RenderDevice.h"
#include "Candle/Render/ShaderLoader.h"

namespace Candle {

	PipelineSpecification TrianglePipeline::CreateSpecification(RenderDevice& renderDevice, vk::Format colorFormat, vk::Format depthFormat) const
	{
		// TODO: Once asset manager is fully implemented, will see if we load the shader from the asset manager
		//		or if this is where we add it to the asset manager
		// Relative to the working directory, which Build.py test and the executables' debugdir set to the repo root
		auto shader = ShaderLoader::LoadShader(m_Name, "Candle/res/shaders/bin/Triangle.spv");
		if (!shader)
			CDL_CORE_PANIC(LogChannel::Render, "{}", shader.error());

		return {
			.Shader = *shader,
			.ColorAttachmentFormats = {colorFormat},
			.DepthAttachmentFormat = depthFormat
			// Rest are defaults for now
		};
	}

}