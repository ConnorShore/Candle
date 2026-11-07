#pragma once

#include "Candle/Render/Pipelines/Pipeline.h"
#include "Candle/Render/Vulkan/BindlessTable.h"
#include "Candle/Render/Vulkan/GPUImage.h"
#include "Candle/Render/Resources/DeletionQueue.h"

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
		std::filesystem::path ShaderDirectory;	// Until shaders come through the asset system
		bool RenderToSwapChain = true;			// If false, the scene renders into each packet's OutputTarget and only the GUI reaches the target
	};

	// Turns one packet into recorded GPU work. Knows nothing about threads, windows or presentation.
	// Threading: render thread only.
	class Renderer
	{
	public:
		Renderer(RenderDevice& renderDevice, GeometryPool& geometryPool, TexturePool& texturePool, BindlessTable& bindlessTable,
					GPUAllocator& allocator, const RendererCreationInfo& info);	// pipelines, bindless tables, render graph
		void Render(const FramePacket& packet, FrameContext& frame, SwapChainImage& target);   // target is imported into the graph; its owner presents it

		// targetExtent is the colour target's real extent, since the depth image must cover the same render area.
		// Only when rendering to the swapchain: otherwise depth follows the output target instead (see MatchDepthToOutput).
		void RecreateTargets(vk::Extent2D targetExtent, GPUAllocator& allocator);

		// Only when not rendering to the swapchain. Resizes depth to the output target's extent if it differs; the old depth image
		// frees once the GPU reaches lastUseValue, the retire value of the last frame submitted, so no frame in flight loses it.
		void MatchDepthToOutput(glm::uvec2 outputExtent, uint64_t lastUseValue);

		// Fixed at construction, so readable from any thread; a render target the scene draws into must have this format
		inline vk::Format GetTargetFormat() const { return m_TargetFormat; }

	private:
		// Where the scene pass draws: a colour attachment the caller has transitioned for it, and its extent
		struct SceneTarget
		{
			vk::ImageView ColorView;
			vk::Extent2D Extent;
		};

	private:
		void CreatePipeline(const RenderPipeline& pipeline); // builds a Pipeline from the description and stores it

		// TODO: Eventaully will be part of render graph; for now, the Renderer owns the depth target and does the passes itself
		void RenderTrianglePipeline(const FramePacket& packet, FrameContext& frame, const SceneTarget& target, const GPU::GlobalPushConstants globalPCs);
		void RenderDebugDrawPipeline(const FramePacket& packet, FrameContext& frame, const SceneTarget& target, vk::AttachmentLoadOp loadOp, const GPU::GlobalPushConstants globalPCs);
		void RenderGuiPipeline(const FramePacket& packet, FrameContext& frame, SwapChainImage& target, vk::AttachmentLoadOp loadOp, const GPU::GlobalPushConstants globalPCs);

		// TODO: Will probably move to some helper or something once the render graph is implemented
		void EnsureCapacity(GPUBuffer& buffer, size_t size, vk::BufferUsageFlags usage, std::string_view name);
	private:
		RenderDevice& m_RenderDevice;
		GeometryPool& m_GeometryPool;
		TexturePool& m_TexturePool;
		BindlessTable& m_BindlessTable;
		GPUAllocator& m_GPUAllocator;
		bool m_RenderToSwapChain;
		vk::Format m_TargetFormat;
		vk::Format m_DepthFormat;	// Chosen once, before m_DepthImage; the depth image and every pipeline's spec both take it
		GPUImage m_DepthImage;	// Shared by every frame in flight, so each frame's first barrier waits for the last one's depth writes
		glm::uvec2 m_DepthExtent;
		DeletionQueue m_RetiredImages;	// Depth images replaced by MatchDepthToOutput, held until the frames using them retire
		std::vector<Pipeline> m_Pipelines;
	};

}