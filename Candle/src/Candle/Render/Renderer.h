#pragma once

#include "Pipeline.h"
#include "BindlessTable.h"
#include "GPUImage.h"

#include <vector>

#include <vulkan/vulkan_raii.hpp>

namespace Candle {

	class RenderDevice;
	class Pipeline;
	class SwapChainImage;
	class GeometryPool;
	class TexturePool;
	class BindlessTable;
	class RenderPipeline;
	class GPUAllocator;

	struct FrameContext;
	struct FramePacket;

	// Turns one packet into recorded GPU work. Knows nothing about threads, windows or presentation.
	// Threading: render thread only.
	class Renderer
	{
	public:
		explicit Renderer(RenderDevice& renderDevice, GeometryPool& geometryPool, TexturePool& texturePool, 
							BindlessTable& bindlessTable, GPUAllocator& allocator, vk::Format targetFormat, vk::Extent2D targetExtent);	// pipelines, bindless tables, render graph
		void Render(const FramePacket& packet, FrameContext& frame, SwapChainImage& target);   // target is imported into the graph; its owner presents it

		// targetExtent is the colour target's real extent, since the depth image must cover the same render area
		void RecreateTargets(vk::Extent2D targetExtent, GPUAllocator& allocator);

	private:
		void CreatePipeline(const RenderPipeline& pipeline); // builds a Pipeline from the description and stores it

	private:
		RenderDevice& m_RenderDevice;
		GeometryPool& m_GeometryPool;
		TexturePool& m_TexturePool;
		BindlessTable& m_BindlessTable;
		vk::Format m_TargetFormat;
		vk::Format m_DepthFormat;	// Chosen once, before m_DepthImage; the depth image and every pipeline's spec both take it
		GPUImage m_DepthImage;	// Shared by every frame in flight, so each frame's first barrier waits for the last one's depth writes
		std::vector<Pipeline> m_Pipelines;
	};

}