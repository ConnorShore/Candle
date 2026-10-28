#pragma once

#include "RenderSpecification.h"
#include "RenderConstants.h"
#include "RenderInstance.h"
#include "RenderDevice.h"
#include "SwapChain.h"
#include "Renderer.h"
#include "FrameContext.h"
#include "RenderCommandPoolManager.h"
#include "GPUAllocator.h"
#include "GPUUploader.h"
#include "GeometryPool.h"
#include "TexturePool.h"
#include "BindlessTable.h"
#include "GPUResourceRegistry.h"
#include "FramePump.h"

#include "Candle/Core/ApplicationSpecification.h"
#include "Candle/Core/Threading/Thread.h"

#include <atomic>
#include <thread>

namespace Candle {

	// TODO: Figure out FrameContext for command buffer recording and submission
	//  Read khronos tutorial for recording command buffer + multi-threaded rendering to help drive design

	// NOTES //
	/*
	* RenderPassManager to manage render graph and synchronization between passes
	* Command pools and buffers are associated with a queue family
	* Will have command pools/buffers per queue family (graphics, transfer, etc) as well as per frame (double/triple buffering) and per thread (multi-threaded command buffer recording)
	*/
	////

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
		RenderManager(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo, Window& window, JobSystem& jobSystem);
		~RenderManager(); // Join render thread, wait for GPU idle (shutdown only), reverse teardown

		void SpawnRenderThread();	// Spawns the render thread, which will run RenderThreadMain() until the application exits (called from the main thread on application startup)
		void JoinRenderThread();	// Joins the render thread, waiting for it to finish (called from the main thread on application shutdown)

		void RegisterAssetHandlers(AssetManager& assetManager);

		// Main thread only. Blocks while the render thread is a full pump behind, then hands the packet this frame's
		// resource requests; fill it and pass it to SubmitFramePacket.
		FramePacket& BeginFramePacket();
		void SubmitFramePacket(FramePacket& packet);

		inline RenderDevice& GetRenderDevice() { return m_RenderDevice; }
		inline GeometryPool& GetGeometryPool() { return m_GeometryPool; }	// Temporary (applicaiton shouldn't see geometry pool)

	private:
		void RenderThreadMain();   // Wait for a frame slot → acquire → Renderer::Render → submit → present
		void RetireAndReleasePacket(FramePacket& packet, uint64_t retireValue);	// Retires its releases, then hands it back to the main thread

		void CreateFrameContexts();
		void RecreateResources(const glm::uvec2& surfaceExtent);

	private:
		RenderSpecification m_Specification;
		Window& m_Window;
		JobSystem& m_JobSystem;
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