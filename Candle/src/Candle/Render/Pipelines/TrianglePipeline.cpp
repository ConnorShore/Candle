#include "cdlpch.h"
#include "TrianglePipeline.h"

#include "Candle/Render/RenderDevice.h"
#include "Candle/Render/ShaderLoader.h"

namespace Candle {

	PipelineSpecification TrianglePipeline::CreateSpecification(RenderDevice& renderDevice, vk::Format colorFormat, vk::Format depthFormat) const
	{
		// Create triangle pipeline here for now, will eventually be created on demand when a pipeline is needed
		// TODO: Once asset manager is fully implemented, will see if we load the shader from the asset manager
		//		or if this is where we add it to the asset manager
		// Relative to the working directory, which Build.py test and the executables' debugdir set to the repo root
		SharedPtr<Shader> shader = ShaderLoader::LoadShader(
			m_Name,
			"Candle/res/shaders/bin/Triangle.spv",
			renderDevice
		);

		return {
			.Shader = shader,
			.ColorAttachmentFormats = {colorFormat},
			.DepthAttachmentFormat = depthFormat
			// Rest are defaults for now
		};
	}

}