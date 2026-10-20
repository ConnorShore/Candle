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

#include "Candle/Core/ApplicationSpecification.h"

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

		void RegisterAssetHandlers(AssetManager& assetManager);

		// Submits a frame packet to the render thread for processing
		void SubmitFrame(FramePacket&& packet);

		// Main thread only. Report what happened to the surface; RenderManager decides whether that means a rebuild
		void OnSurfaceResized(const glm::uvec2& pixelExtent);
		void OnSurfaceMinimized(bool minimized);

		// TODO: This will be ran by the render thread in here once its implemented. will become private but its public for now so
		// we can call it from application directly to get the triangle to render
		void RenderThreadMain();   // Wait for a frame slot → acquire → Renderer::Render → submit → present

		inline RenderDevice& GetRenderDevice() { return m_RenderDevice; }
		inline GeometryPool& GetGeometryPool() { return m_GeometryPool; }	// Temporary (applicaiton shouldn't see geometry pool)

	private:
		void CreateFrameContexts();
		void RecreateResources();

	private:
		RenderSpecification m_Specification;
		Window& m_Window;
		JobSystem& m_JobSystem;
		RenderInstance m_RenderInstance;
		RenderDevice m_RenderDevice;
		SwapChain m_SwapChain;
		GPUAllocator m_GPUAllocator;
		GPUUploader m_GPUUploader;	// After the allocator it stages through, before the pools that upload with it
		GeometryPool m_GeometryPool;
		Renderer m_Renderer;
		RenderCommandPoolManager m_CommandPoolManager;

		std::array<FrameContext, kMaxFramesInFlight> m_FrameContexts;
		uint32_t m_FrameIndex{ 0 };

		glm::uvec2 m_SurfaceExtent;
		bool m_SurfaceMinimized{ false };

		std::vector<FramePacket> m_FrameQueue; // Queue of frame packets to be processed by the render thread (tem
	};

}