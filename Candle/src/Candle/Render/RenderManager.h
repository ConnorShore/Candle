#pragma once

#include "RenderSpecification.h"
#include "RenderInstance.h"
#include "RenderDevice.h"

#include "Candle/Core/ApplicationSpecification.h"

namespace Candle {

	class Window;

	// Holds RenderInstance, RenderDevice, Swapchain, Renderer, Renderthread, etc
	class RenderManager
	{
	public:
		RenderManager(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo, Window& window);
		~RenderManager();									 // Tears down in reverse; must wait for the GPU first once anything is submitted

		//void OnPlatformEvent(const PlatformEvent& event);  // Resize/minimize: marks the swapchain stale
		//void SubmitFrame(/* FramePacket&& */);             // Simulation never touches the packet again

		inline RenderDevice& GetRenderDevice() { return m_RenderDevice; }

	private:
		RenderSpecification m_Specification;
		Window& m_Window;
		RenderInstance m_RenderInstance;
		RenderDevice m_RenderDevice;
	};

}