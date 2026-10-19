#include "cdlpch.h"
#include "TrianglePipeline.h"

#include "Candle/Render/RenderDevice.h"
#include "Candle/Render/ShaderLoader.h"

namespace Candle {

	PipelineSpecification TrianglePipeline::CreateSpecification(RenderDevice& renderDevice, vk::Format format) const
	{
		// Create triangle pipeline here for now, will eventually be created on demand when a pipeline is needed
		// TODO: Once asset manager is fully implemented, will see if we load the shader from the asset manager
		//		or if this is where we add it to the asset manager
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