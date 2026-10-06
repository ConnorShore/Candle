#pragma once

#include "RenderSpecification.h"
#include "RenderInstance.h"

#include "Candle/Core/ApplicationSpecification.h"

namespace Candle {

	class Window;

	// Holds RenderInstance, RenderDevice, Swapchain, Renderer, Renderthread, etc
	class RenderManager
	{
	public:
		RenderManager(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo, Window& window);
		~RenderManager();									 // Waits for the GPU, then tears down in reverse

		//void OnPlatformEvent(const PlatformEvent& event);  // Resize/minimize: marks the swapchain stale
		//void SubmitFrame(/* FramePacket&& */);             // Simulation never touches the packet again

		//RenderDevice& GetDevice();                         // Startup wiring only, e.g. the asset system

	private:
		RenderSpecification m_Specification;
		Window& m_Window;
		RenderInstance m_RenderInstance;
		ApplicationInfo m_AppInfo;
	};

}