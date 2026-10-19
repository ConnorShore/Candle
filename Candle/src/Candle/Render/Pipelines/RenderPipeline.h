#pragma once

#include "Candle/Render/PipelineSpecification.h"

#include <string>

namespace Candle {

	class RenderDevice;

	// Describes how to build one Pipeline; the Renderer owns the Pipeline it produces.
	// Threading: CreateSpecification loads shaders from disk, so call it at init or on a loading thread, never mid-frame
	class RenderPipeline
	{
	public:
		virtual ~RenderPipeline() = default;

		virtual PipelineSpecification CreateSpecification(RenderDevice& renderDevice, vk::Format format) const = 0;

		inline const std::string& GetName() const { return m_Name; }

	protected:
		explicit RenderPipeline(std::string name) : m_Name(std::move(name)) {}

	protected:
		std::string m_Name;	// Name for debugging
	};

}
