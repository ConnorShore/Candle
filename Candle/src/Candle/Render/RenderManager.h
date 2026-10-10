#pragma once

#include "RenderSpecification.h"
#include "RenderConstants.h"
#include "RenderInstance.h"
#include "RenderDevice.h"
#include "SwapChain.h"
#include "Renderer.h"
#include "FrameContext.h"
#include "RenderCommandPoolManager.h"

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

	// Holds RenderInstance, RenderDevice, Swapchain, Renderer, Renderthread, etc
	// Owns the GPU objects and the render thread; the only render type Application sees.
	// Threading: constructed, destroyed and called on the main thread only; everything it owns runs on the render thread.
	class RenderManager
	{
	public:
		RenderManager(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo, Window& window, JobSystem& jobSystem);
		~RenderManager(); // Join render thread, wait for GPU idle (shutdown only), reverse teardown

		//void OnPlatformEvent(const PlatformEvent& event);  // Resize/minimize: marks the swapchain stale
		//void SubmitFrame(/* FramePacket&& */);             // Simulation never touches the packet again

		inline RenderDevice& GetRenderDevice() { return m_RenderDevice; }

		// TODO: This will be ran by the render thread in here once its implemented. will become private but its public for now so
		// we can call it from application directly to get the triangle to render
		void RenderThreadMain();   // Wait for a frame slot → acquire → Renderer::Render → submit → present

	private:
		struct FrameSync
		{
			uint32_t FrameIndex{ 0 };
			vk::raii::Semaphore TimelineSemaphore{ nullptr };
			uint64_t TimelineValue{ 0 };
			std::vector<vk::raii::Semaphore> ImageAvailableSemaphores;
			std::vector<vk::raii::Fence> InFlightFences;
		};

	private:
		RenderSpecification m_Specification;
		Window& m_Window;
		JobSystem& m_JobSystem;
		RenderInstance m_RenderInstance;
		RenderDevice m_RenderDevice;
		SwapChain m_SwapChain;
		Renderer m_Renderer;
		RenderCommandPoolManager m_CommandPoolManager;

		std::array<FrameContext, kMaxFramesInFlight> m_FrameContexts;

		FrameSync m_FrameSync;
	};

}