#pragma once

#include "Candle/Render/RenderSpecification.h"
#include "Candle/Render/RenderConstants.h"
#include "Candle/Render/Vulkan/RenderInstance.h"
#include "Candle/Render/Vulkan/RenderDevice.h"
#include "Candle/Render/Vulkan/SwapChain.h"
#include "Candle/Render/Renderer.h"
#include "Candle/Render/FrameContext.h"
#include "Candle/Render/Vulkan/RenderCommandPoolManager.h"
#include "Candle/Render/Vulkan/GPUAllocator.h"
#include "Candle/Render/Resources/GPUUploader.h"
#include "Candle/Render/Resources/GeometryPool.h"
#include "Candle/Render/Resources/TexturePool.h"
#include "Candle/Render/Vulkan/BindlessTable.h"
#include "Candle/Render/Resources/GPUResourceRegistry.h"
#include "Candle/Render/FramePump.h"

#include "Candle/Core/ApplicationInfo.h"
#include "Candle/Core/Threading/Thread.h"

#include <atomic>
#include <thread>

namespace Candle {

	class Window;
	class JobSystem;
	class RenderCommandPoolManager;
	class AssetManager;

	// Holds RenderInstance, RenderDevice, Swapchain, Renderer, Renderthread, etc
	// Owns the GPU objects and the render thread; the only render type Application sees.
	// Threading: constructed, destroyed and called on the main thread only; everything it owns runs on the render thread.
	class RenderManager
	{
	public:
		RenderManager(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo, const std::filesystem::path& engineAssetDir, Window& window, JobSystem& jobSystem);
		~RenderManager(); // Join render thread, wait for GPU idle (shutdown only), reverse teardown

		void SpawnRenderThread();	// Spawns the render thread, which will run RenderThreadMain() until the application exits (called from the main thread on application startup)
		void JoinRenderThread();	// Joins the render thread, waiting for it to finish (called from the main thread on application shutdown)

		void RegisterAssetHandlers(AssetManager& assetManager);

		// Main thread only. Blocks while the render thread is a full pump behind, then hands the packet this frame's
		// resource requests; fill it and pass it to SubmitFramePacket.
		FramePacket& BeginFramePacket();
		void SubmitFramePacket(FramePacket& packet);

		inline RenderDevice& GetRenderDevice() { return m_RenderDevice; }
		inline GeometryPool& GetGeometryPool() { return m_GeometryPool; }	// For tests only; the application goes through asset handlers

	private:
		void RenderThreadMain();   // Wait for a frame slot → acquire → Renderer::Render → submit → present
		void RetireAndReleasePacket(FramePacket& packet, uint64_t retireValue);	// Retires its releases, then hands it back to the main thread

		void CreateFrameContexts();
		void RecreateResources(const glm::uvec2& surfaceExtent);

	private:
		RenderSpecification m_Specification;
		FramePump m_FramePump;	// Owned here, so a pump never exists without a render thread to drain it
		RenderInstance m_RenderInstance;
		RenderDevice m_RenderDevice;
		SwapChain m_SwapChain;
		GPUAllocator m_GPUAllocator;
		GPUUploader m_GPUUploader;	// After the allocator it stages through, before the pools that upload with it
		GPUResourceRegistry m_ResourceRegistry;	// Before the pools that register with it
		BindlessTable m_BindlessTable;	// Before the pools, so it outlives everything that writes into it
		GeometryPool m_GeometryPool;
		TexturePool m_TexturePool;
		Renderer m_Renderer;
		RenderCommandPoolManager m_CommandPoolManager;
		Thread m_RenderThread;

		std::array<FrameContext, kMaxFramesInFlight> m_FrameContexts;
		uint32_t m_FrameIndex{ 0 };
	};

}