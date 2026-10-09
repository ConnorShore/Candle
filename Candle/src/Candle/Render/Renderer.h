#pragma once

#include "Pipeline.h"

#include <vector>

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	class Pipeline;
	struct PipelineSpecification;

	class RenderDevice;

	// Turns one packet into recorded GPU work. Knows nothing about threads, windows or presentation.
	// Threading: render thread only.
	class Renderer
	{
	public:
		explicit Renderer(RenderDevice& renderDevice, vk::Format targetFormat);        // pipelines, bindless tables, render graph
		//void Render(const FramePacket& packet, FrameContext& frame, const Image& target);       // target is imported into the graph; its owner presents it

	private:
		void CreatePipeline(PipelineSpecification& spec); // create pipelines for all known shaders

	private:
		RenderDevice& m_RenderDevice;
		vk::Format m_TargetFormat;

		// Eventually this will go in a PipelineLibrary and the RenderPa
		std::vector<Pipeline> m_Pipelines;
	};

}