#include "cdlpch.h"
#include "DebugDrawPipeline.h"

#include "Candle/Render/Vulkan/RenderDevice.h"
#include "Candle/Render/Pipelines/ShaderLoader.h"

namespace Candle {

	PipelineSpecification DebugDrawPipeline::CreateSpecification(RenderDevice& renderDevice, vk::Format colorFormat, vk::Format depthFormat) const
	{
		auto shader = ShaderLoader::LoadShader(m_Name, m_ShaderPath);
		if (!shader)
			CDL_CORE_PANIC(LogChannel::Render, "{}", shader.error());

		return {
			.Shader = *shader,
			.Topology = PrimitiveTopology::LineList,
			.ColorAttachmentFormats = { colorFormat },
			.DepthAttachmentFormat = depthFormat,
			.DebugName = m_Name
			// Rest are defaults for now
		};
	}

}