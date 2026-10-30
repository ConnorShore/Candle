#include "cdlpch.h"
#include "TrianglePipeline.h"

#include "Candle/Render/Vulkan/RenderDevice.h"
#include "Candle/Render/Pipelines/ShaderLoader.h"

namespace Candle {

	PipelineSpecification TrianglePipeline::CreateSpecification(RenderDevice& renderDevice, vk::Format colorFormat, vk::Format depthFormat) const
	{
		// TODO: Once asset manager is fully implemented, will see if we load the shader from the asset manager
		//		or if this is where we add it to the asset manager
		auto shader = ShaderLoader::LoadShader(m_Name, m_ShaderPath);
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