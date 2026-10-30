#pragma once

#include "Candle/Render/Pipelines/RenderPipeline.h"

namespace Candle {

	class TrianglePipeline : public RenderPipeline
	{
	public:
		explicit TrianglePipeline(const std::filesystem::path& shaderDirectory)
			: RenderPipeline("BasicTriangle"), m_ShaderPath(shaderDirectory / "Triangle.spv") {}

		PipelineSpecification CreateSpecification(RenderDevice& renderDevice, vk::Format colorFormat, vk::Format depthFormat) const override;

	private:
		std::filesystem::path m_ShaderPath;
	};

}
