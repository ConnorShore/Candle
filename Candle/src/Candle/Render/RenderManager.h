#pragma once

#include "RenderSpecification.h"

#include "RenderInstance.h"
#include "RenderDevice.h"
#include "SwapChain.h"
#include "Renderer.h"

#include "Candle/Core/ApplicationSpecification.h"

namespace Candle {

	class Window;

	// Holds RenderInstance, RenderDevice, Swapchain, Renderer, Renderthread, etc
	// Owns the GPU objects and the render thread; the only render type Application sees.
	// Threading: constructed, destroyed and called on the main thread only; everything it owns runs on the render thread.
	class RenderManager
	{
	public:
		RenderManager(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo, Window& window);
		~RenderManager(); // Join render thread, wait for GPU idle (shutdown only), reverse teardown

		//void OnPlatformEvent(const PlatformEvent& event);  // Resize/minimize: marks the swapchain stale
		//void SubmitFrame(/* FramePacket&& */);             // Simulation never touches the packet again

		inline RenderDevice& GetRenderDevice() { return m_RenderDevice; }

	private:
		//void RenderThreadMain();   // Wait for a frame slot → acquire → Renderer::Render → submit → present

	private:
		RenderSpecification m_Specification;
		Window& m_Window;
		RenderInstance m_RenderInstance;
		RenderDevice m_RenderDevice;
		SwapChain m_SwapChain;
		Renderer m_Renderer;
	};

}