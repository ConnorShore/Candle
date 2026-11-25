#pragma once

#include "Candle/Render/Pipelines/PipelineStore.h"
#include "Candle/Render/Vulkan/BindlessTable.h"
#include "Candle/Render/Vulkan/GPUImage.h"
#include "Candle/Render/Resources/DeletionQueue.h"
#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"
#include "Candle/Render/FrameGraph/FrameGraph.h"
#include "Candle/Render/FrameGraph/RenderModule.h"

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
					GPUAllocator& allocator, const RendererCreationInfo& info);

		void Render(const FramePacket& packet, FrameContext& frame, SwapChainImage& target);

		// The last Render's compiled graph; only until the next Render resets it
		inline void CaptureFrameGraph(FrameGraphSnapshot& out) const { m_FrameGraph.CaptureSnapshot(out); }

		// Fixed at construction, so readable from any thread; a render target the scene draws into must have this format
		inline vk::Format GetTargetFormat() const { return m_TargetFormat; }

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

		std::vector<ScopedPtr<RenderModule>> m_RenderModules;
	};

}