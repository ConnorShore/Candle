#pragma once

#include "Candle/Render/Pipelines/RenderPipeline.h"

namespace Candle {

	class DebugDrawPipeline : public RenderPipeline
	{
	public:
		explicit DebugDrawPipeline(const std::filesystem::path& shaderDirectory)
			: RenderPipeline("DebugDraw"), m_ShaderPath(shaderDirectory / "DebugDraw.spv") {
		}

		PipelineSpecification CreateSpecification(RenderDevice& renderDevice, vk::Format colorFormat, vk::Format depthFormat) const override;

	private:
		std::filesystem::path m_ShaderPath;
	};

}