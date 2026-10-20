#include "cdlpch.h"
#include "RenderManager.h"
#include "ShaderLoader.h"
#include "Shader.h"
#include "FramePacket.h"

#include "Candle/Core/Window.h"
#include "Candle/Core/Job/JobSystem.h"
#include "Candle/Core/Memory/ScopedPtr.h"
#include "Candle/Platform/Platform.h"
#include "Candle/Asset/AssetManager.h"

#include "Candle/Render/Assets/MeshAssetHandler.h"
#include "Candle/Render/ShaderInterop/FrameData.h"

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
		, m_GPUAllocator({ renderSpec.VulkanVersion, &m_RenderDevice, &m_RenderInstance })
		, m_GPUUploader(m_RenderDevice, m_GPUAllocator)
		, m_GeometryPool(m_GPUAllocator, m_GPUUploader)
		, m_Renderer(m_RenderDevice, m_GeometryPool, m_SwapChain.GetSurfaceFormat().format)
		, m_CommandPoolManager(m_RenderDevice, m_JobSystem.GetNumWorkers())
		, m_SurfaceExtent(window.GetSize())
	{
		CreateFrameContexts();
	}

	RenderManager::~RenderManager()
	{
		// Members are destroyed after this body returns, and the GPU may still be using them
		m_RenderDevice.WaitIdle();
	}

	void RenderManager::RegisterAssetHandlers(AssetManager& assetManager)
	{
		assetManager.RegisterAssetHandler(ScopedPtr<MeshAssetHandler>::Create(m_GeometryPool));
	}

	void RenderManager::SubmitFrame(FramePacket&& packet)
	{
		m_FrameQueue.push_back(std::move(packet));
	}

	void RenderManager::OnSurfaceResized(const glm::uvec2& pixelExtent)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "RenderManager::OnSurfaceResized() must be called from the main thread.");
		m_SurfaceExtent = pixelExtent;
	}

	void RenderManager::OnSurfaceMinimized(bool minimized)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "RenderManager::OnSurfaceMinimized() must be called from the main thread.");
		m_SurfaceMinimized = minimized;
	}

	void RenderManager::RenderThreadMain()
	{
		// A minimized surface reports a zero extent, and a swapchain can't be created at that size
		if (m_SurfaceMinimized || m_SurfaceExtent.x == 0 || m_SurfaceExtent.y == 0)
			return;

		// Wait for a frame slot → acquire → Renderer::Render → submit → present
		FrameContext& frameContext = m_FrameContexts[m_FrameIndex];

		// Once the GPU reaches the slot's retire value, its command pools and acquire semaphore are free to reuse
		m_RenderDevice.WaitFor(QueueType::Graphics, frameContext.RetireValue);
		frameContext.CommandPool.reset();
		m_CommandPoolManager.BeginFrame(m_FrameIndex);

		// Rebuild before acquiring: an image acquired and then abandoned leaves its acquire semaphore signaled
		if (!m_SwapChain.IsValid(m_SurfaceExtent))
			RecreateResources();

		// Acquire the next image from the swapchain; out of date signals nothing, so returning leaves the semaphore reusable
		auto [result, imageIndex] = m_SwapChain.AquireNextImage(m_FrameIndex);
		if (result == vk::Result::eErrorOutOfDateKHR)
		{
			RecreateResources();
			return;
		}
		else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		{
			// On other success codes than eSuccess and eSuboptimalKHR we just throw an exception.
			// On any error code, aquireNextImage already threw an exception.
			CDL_CORE_ASSERT(result == vk::Result::eTimeout || result == vk::Result::eNotReady, "Failed to quire swap chain image!");
			throw std::runtime_error("Failed to acquire swap chain image!");
		}

		// IN FUTURE: Kick of compute jobs async from rendering (usually for frame N+1) while gpu renders frame N

		// Get the next frame packet to render (this will be replaced with a more sophisticated frame pumping system in the future)
		FramePacket packet = m_FrameQueue.front();
		m_FrameQueue.erase(m_FrameQueue.begin());

		// Record command buffers for rendering
		m_Renderer.Render(packet, frameContext, m_SwapChain.GetImage(imageIndex));

		// Submit the command buffer to the graphics queue, waiting on the acquire semaphore and signaling the present semaphore
		vk::CommandBufferSubmitInfo commandBufferInfo{ .commandBuffer = *frameContext.CommandBuffer };
		vk::SemaphoreSubmitInfo acquireWait = m_SwapChain.GetAcquireWait(m_FrameIndex);
		vk::SemaphoreSubmitInfo presentSignal = m_SwapChain.GetPresentSignal(imageIndex);
		frameContext.RetireValue = m_RenderDevice.Submit(QueueType::Graphics, {
			.CommandBuffers = { &commandBufferInfo, 1 },
			.Waits = { &acquireWait, 1 },
			.Signals = { &presentSignal, 1 } });

		// Present the image on the swapchain
		result = m_SwapChain.Present(imageIndex);
		if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR)
		{
			RecreateResources();
			return;
		}
		else
			CDL_CORE_ASSERT(result == vk::Result::eSuccess, "Should be no other codes than eSuccess");

		m_FrameIndex = (m_FrameIndex + 1) % kMaxFramesInFlight;
	}

	void RenderManager::CreateFrameContexts()
	{
		vk::raii::Device& device = m_RenderDevice.GetDevice();
		vk::CommandPoolCreateInfo poolInfo{ .queueFamilyIndex = m_RenderDevice.GetQueueFamily(QueueType::Graphics) };

		for (FrameContext& frameContext : m_FrameContexts)
		{
			frameContext.CommandPool = vk::raii::CommandPool(device, poolInfo);

			vk::CommandBufferAllocateInfo allocInfo{
				.commandPool = *frameContext.CommandPool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = 1
			};
			frameContext.CommandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());

			// Written by the CPU every frame and read once per draw, so host-visible rather than staged into device memory
			frameContext.FrameData = m_GPUAllocator.CreateBuffer({
				.m_Size = sizeof(GPU::FrameData),
				.m_Usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
				.m_Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT });
		}
	}

	void RenderManager::RecreateResources()
	{
		m_RenderDevice.WaitIdle();

		// The reported extent, not Window::GetSize(): Window is main thread only, and this will run on the render thread
		m_SwapChain.Recreate(m_SurfaceExtent);
	}

}