#pragma once

#include "RenderPipeline.h"

namespace Candle {

	class TrianglePipeline : public RenderPipeline
	{
	public:
		TrianglePipeline() : RenderPipeline("BasicTriangle") {}

		PipelineSpecification CreateSpecification(RenderDevice& renderDevice, vk::Format format) const override;

	};

}
