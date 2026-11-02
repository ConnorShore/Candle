#pragma once

#include "Candle/Render/Pipelines/RenderPipeline.h"

namespace Candle {

	// TODO: Setup like TrianglePipeline, but once its working properly
	// abstract out pipeline stuff a bit better (i.e. shaderPath should be in RenderPipeline)
	// and probably have RenderPipeline do shader loading as well (see what Vulkan book does for impl as well)

	class GuiPipeline : public RenderPipeline
	{
	public:
		explicit GuiPipeline(const std::filesystem::path& shaderDirectory)
			: RenderPipeline("Gui"), m_ShaderPath(shaderDirectory / "Gui.spv") {
		}

		PipelineSpecification CreateSpecification(RenderDevice& renderDevice, vk::Format colorFormat, vk::Format depthFormat) const override;

	private:
		std::filesystem::path m_ShaderPath;
	};

}
