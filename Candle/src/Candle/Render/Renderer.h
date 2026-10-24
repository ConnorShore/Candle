#pragma once

#include "Pipeline.h"

#include <vector>

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	class RenderDevice;
	class Pipeline;
	class SwapChainImage;
	class GeometryPool;
	class RenderPipeline;

	struct FrameContext;
	struct FramePacket;

	// Turns one packet into recorded GPU work. Knows nothing about threads, windows or presentation.
	// Threading: render thread only.
	class Renderer
	{
	public:
		explicit Renderer(RenderDevice& renderDevice, GeometryPool& geometryPool, vk::Format targetFormat);				// pipelines, bindless tables, render graph
		void Render(const FramePacket& packet, FrameContext& frame, SwapChainImage& target);   // target is imported into the graph; its owner presents it

	private:
		void CreatePipeline(const RenderPipeline& pipeline); // builds a Pipeline from the description and stores it

	private:
		RenderDevice& m_RenderDevice;
		GeometryPool& m_GeometryPool;
		vk::Format m_TargetFormat;

		// Eventually this will go in a PipelineLibrary and the RenderPass
		std::vector<Pipeline> m_Pipelines;


	};

}