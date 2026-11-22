#pragma once

#include "Candle/Render/Pipelines/PipelineStore.h"
#include "Candle/Render/Vulkan/BindlessTable.h"
#include "Candle/Render/Vulkan/GPUImage.h"
#include "Candle/Render/Resources/DeletionQueue.h"
#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"
#include "Candle/Render/FrameGraph/FrameGraph.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <vector>
#include <string_view>

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
	class GPUBuffer;

	struct FrameContext;
	struct FramePacket;

	struct RendererCreationInfo
	{
		vk::Format TargetFormat;
		vk::Extent2D TargetExtent;
		std::filesystem::path ShaderDirectory;			// Until shaders come through the asset system
		std::filesystem::path PipelineCacheDirectory;	// Where to read/write the pipeline cache file	
		bool RenderToSwapChain = true;					// If false, the scene renders into each packet's OutputTarget and only the GUI reaches the target
	};

	// Turns one packet into recorded GPU work. Knows nothing about threads, windows or presentation.
	// Threading: render thread only.
	class Renderer
	{
	public:
		Renderer(RenderDevice& renderDevice, GeometryPool& geometryPool, TexturePool& texturePool, BindlessTable& bindlessTable,
					GPUAllocator& allocator, const RendererCreationInfo& info);	// pipelines, bindless tables, render graph
		void Render(const FramePacket& packet, FrameContext& frame, SwapChainImage& target);   // target is imported into the graph; its owner presents it

		// Fixed at construction, so readable from any thread; a render target the scene draws into must have this format
		inline vk::Format GetTargetFormat() const { return m_TargetFormat; }

	private:
		// Eventually will be in a SceneModule that holds scene render passes
		struct ScenePassData
		{
			FGTextureHandle Color;
			FGTextureHandle Depth;
		};

		struct ComputePassData
		{
			FGTextureHandle Output;
		};

	private:
		void CreatePipeline(const RenderPipeline& pipeline); // builds a Pipeline from the description and stores it

		const ComputePassData& AddTestComputePass(const GPU::GlobalPushConstants& global);
		const ScenePassData& AddScenePass(const FramePacket& packet, FGTextureHandle target, FGTextureHandle computeTex, const GPU::GlobalPushConstants& global);
		void AddDebugDrawPass(const FramePacket& packet, FrameContext& frame, const ScenePassData& scene, const GPU::GlobalPushConstants& global);
		void AddGuiPass(const FramePacket& packet, FrameContext& frame, FGTextureHandle swapChain, FGTextureHandle sceneColor, const GPU::GlobalPushConstants& global);

		void EnsureCapacity(GPUBuffer& buffer, size_t size, vk::BufferUsageFlags usage, std::string_view name);
	private:
		RenderDevice& m_RenderDevice;
		GeometryPool& m_GeometryPool;
		TexturePool& m_TexturePool;
		BindlessTable& m_BindlessTable;
		GPUAllocator& m_GPUAllocator;
		PipelineStore m_PipelineStore;
		FrameGraph m_FrameGraph;

		bool m_RenderToSwapChain;
		vk::Format m_TargetFormat;
		vk::Format m_DepthFormat;	// Chosen once, before m_DepthImage; the depth image and every pipeline's spec both take it
		glm::vec2 m_LineWidthRange;

		// Temporary till frame graph modules are implemented
		GPUPipelineHandle m_TrianglePipeline, m_DebugDrawPipeline, m_GuiPipeline;
		GPUPipelineHandle m_ComputePipeline;
	};

}