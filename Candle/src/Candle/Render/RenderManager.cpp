#include "cdlpch.h"
#include "RenderManager.h"
#include "ShaderLoader.h"
#include "Shader.h"
#include "FramePacket.h"

#include "Candle/Core/Window.h"
#include "Candle/Core/Job/JobSystem.h"

namespace Candle {

	namespace {

		RenderInstanceSpecification CreateRenderInstanceSpec(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo)
		{
			return {
				.VulkanVersion = renderSpec.VulkanVersion,
				.ValidationSpec = renderSpec.ValidationSpec,
				.ApplicationName = appInfo.Name,
				.ApplicationVersion = appInfo.Version
			};
		}

	}

	RenderManager::RenderManager(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo, Window& window, JobSystem& jobSystem) 
		: m_Specification(renderSpec)
		, m_Window(window)
		, m_JobSystem(jobSystem)
		, m_RenderInstance(CreateRenderInstanceSpec(renderSpec, appInfo))
		, m_RenderDevice(m_RenderInstance, m_Specification)
		, m_SwapChain(m_RenderInstance, m_RenderDevice, window)
		, m_Renderer(m_RenderDevice, m_SwapChain.GetSurfaceFormat().format)
		, m_CommandPoolManager(m_RenderDevice, m_JobSystem.GetNumWorkers())
	{
	}

	RenderManager::~RenderManager()
	{
	}

	void RenderManager::RenderThreadMain()
	{
		// Wait for a frame slot → acquire → Renderer::Render → submit → present
		auto& frameContext = m_FrameContexts[m_FrameSync.FrameIndex];

		auto fenceResult = m_RenderDevice.GetDevice().waitForFences(*m_FrameSync.InFlightFences[m_FrameSync.FrameIndex], vk::True, UINT64_MAX);
		if (fenceResult != vk::Result::eSuccess)
		{
			// Should I handle this differently?
			throw std::runtime_error("Failed to wait for fence!");
		}

		m_CommandPoolManager.BeginFrame(m_FrameSync.FrameIndex);

		// Acquire the next image from the swapchain
		auto [result, imageIndex] = m_SwapChain.AquireNextImage(m_FrameSync.ImageAvailableSemaphores[m_FrameSync.FrameIndex]);
		//TODO: if (result == vk::Result::eErrorOutOfDateKHR || framebufferResized)
		//{
		//	framebufferResized = false;
		//	recreateSwapChain();
		//	return;
		//}
		//if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		//{
		//	assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
		//	throw std::runtime_error("failed to acquire swap chain image!");
		//}

		// IN FUTURE: Kick of compute jobs async from rendering (usually for frame N+1) while gpu renders frame N

		// Record command buffers for rendering (this may be Renderer::Render?)
		FramePacket packet{};	// Temporary
		m_Renderer.Render(packet, frameContext, m_SwapChain.GetImage(imageIndex));

		// Collect submit info and submit to the graphics queue

		// Wait for the GPU to finish rendering and present the image

		// Present the image to the swapchain

		// Update the retire value and frame index for the next frame
		frameContext.RetireValue = ++m_FrameSync.TimelineValue;
		m_FrameSync.FrameIndex = (m_FrameSync.FrameIndex + 1) % kMaxFramesInFlight;
	}

}