#include "cdlpch.h"
#include "RenderManager.h"
#include "ShaderLoader.h"
#include "Shader.h"
#include "FramePacket.h"

#include "Candle/Core/Window.h"
#include "Candle/Core/Profiler.h"
#include "Candle/Core/Job/JobSystem.h"
#include "Candle/Core/Memory/ScopedPtr.h"
#include "Candle/Platform/Platform.h"
#include "Candle/Asset/AssetManager.h"
#include "Candle/Render/FramePump.h"

#include "Candle/Render/Assets/MeshAssetHandler.h"
#include "Candle/Render/Assets/TextureHandler.h"
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
		, m_BindlessTable(m_RenderDevice)
		, m_GeometryPool(m_GPUAllocator, m_GPUUploader, m_ResourceRegistry)
		, m_TexturePool(m_GPUAllocator, m_GPUUploader, m_ResourceRegistry)
		, m_Renderer(m_RenderDevice, m_GeometryPool, m_TexturePool, m_BindlessTable, m_SwapChain.GetSurfaceFormat().format)
		, m_CommandPoolManager(m_RenderDevice, m_JobSystem.GetNumWorkers())
		, m_RenderThread({ .Name = "Candle-RenderThread", .Priority = ThreadPriority::High })
	{
		CreateFrameContexts();
	}

	RenderManager::~RenderManager()
	{
		// Members are destroyed after this body returns, and the GPU may still be using them
		m_RenderDevice.WaitIdle();

		JoinRenderThread();
	}

	void RenderManager::SpawnRenderThread()
	{
		m_RenderThread.Start([this](std::stop_token stoken){
			while (!stoken.stop_requested()) {
				RenderThreadMain();
				CDL_PROFILE_FRAME_NAMED("RenderFrame");
			}

			CDL_CORE_INFO(LogChannel::Render, "Render Thread Ending");
		});
	}

	void RenderManager::JoinRenderThread()
	{
		// If render thread already joined, don't try to join again
		if (!m_RenderThread.IsJoinable())
			return;
		
		m_RenderThread.RequestStop();
		CDL_CORE_INFO(LogChannel::Render, "Render Thread Join Requested");

		// The main thread has stopped submitting packets, so wake the render thread if it is waiting for one
		m_FramePump.Close();

		// Wait for render related GPU work to finish
		m_RenderDevice.WaitIdle();

		m_RenderThread.Join();
		CDL_CORE_INFO(LogChannel::Render, "Render Thread Joined");
	}

	void RenderManager::RegisterAssetHandlers(AssetManager& assetManager)
	{
		assetManager.RegisterAssetHandler(ScopedPtr<MeshAssetHandler>::Create(m_GeometryPool));
		assetManager.RegisterAssetHandler(ScopedPtr<TextureHandler>::Create(m_TexturePool, m_BindlessTable));
	}

	FramePacket& RenderManager::BeginFramePacket()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "RenderManager::BeginFramePacket() must be called from the main thread.");

		FramePacket& packet = m_FramePump.AquireForWriting();
		m_GPUUploader.FreeCompletedStagingBuffers();
		m_ResourceRegistry.ReturnRecycledSlotsToPools(packet);
		m_ResourceRegistry.MoveReleasedSlotsIntoPacket(packet);
		return packet;
	}

	void RenderManager::SubmitFramePacket(FramePacket& packet)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "RenderManager::SubmitFramePacket() must be called from the main thread.");
		m_FramePump.Submit(packet);
	}

	void RenderManager::RenderThreadMain()
	{
		CDL_PROFILE_SCOPE("RenderManager::RenderThreadMain");
		// A minimized surface reports a zero extent, and a swapchain can't be created at that size
		FramePacket* packet = m_FramePump.AquireForReading();
		if (!packet)
			return;	// Closed by JoinRenderThread

		CDL_PROFILE_SCOPE_METADATA(packet->FrameIndex);

		// A minimized surface reports a zero extent, and a swapchain can't be created at that size
		glm::uvec2 surfaceSize = packet->SurfaceSize;
		if (packet->Minimized || surfaceSize.x == 0 || surfaceSize.y == 0)
		{
			RetireAndReleasePacket(*packet, m_RenderDevice.Submit(QueueType::Graphics, {}));
			return;
		}

		// Wait for a frame slot → acquire → Renderer::Render → submit → present
		FrameContext& frameContext = m_FrameContexts[m_FrameIndex];

		// Once the GPU reaches the slot's retire value, its command pools and acquire semaphore are free to reuse
		m_RenderDevice.WaitFor(QueueType::Graphics, frameContext.RetireValue);
		frameContext.CommandPool.reset();
		m_CommandPoolManager.BeginFrame(m_FrameIndex);

		// Rebuild before acquiring: an image acquired and then abandoned leaves its acquire semaphore signaled
		if (!m_SwapChain.IsValid(surfaceSize))
			RecreateResources(surfaceSize);

		// Acquire the next image from the swapchain; out of date signals nothing, so returning leaves the semaphore reusable
		auto [result, imageIndex] = m_SwapChain.AquireNextImage(m_FrameIndex);
		if (result == vk::Result::eErrorOutOfDateKHR)
		{
			RecreateResources(surfaceSize);

			// Nothing drew the packet, but its releases must still retire; an empty submit orders after all earlier work
			RetireAndReleasePacket(*packet, m_RenderDevice.Submit(QueueType::Graphics, {}));
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

		// Record command buffers for rendering
		m_Renderer.Render(*packet, frameContext, m_SwapChain.GetImage(imageIndex));

		// Submit the command buffer to the graphics queue, waiting on the acquire semaphore and signaling the present semaphore
		vk::CommandBufferSubmitInfo commandBufferInfo{ .commandBuffer = *frameContext.CommandBuffer };
		vk::SemaphoreSubmitInfo acquireWait = m_SwapChain.GetAcquireWait(m_FrameIndex);
		vk::SemaphoreSubmitInfo presentSignal = m_SwapChain.GetPresentSignal(imageIndex);
		frameContext.RetireValue = m_RenderDevice.Submit(QueueType::Graphics, {
			.CommandBuffers = { &commandBufferInfo, 1 },
			.Waits = { &acquireWait, 1 },
			.Signals = { &presentSignal, 1 } });

		// The packet's releases retire with the submit that recorded it, which covers every earlier packet too
		RetireAndReleasePacket(*packet, frameContext.RetireValue);

		// Present the image on the swapchain
		result = m_SwapChain.Present(imageIndex);
		if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR)
		{
			RecreateResources(surfaceSize);
			return;
		}
		else
			CDL_CORE_ASSERT(result == vk::Result::eSuccess, "Should be no other codes than eSuccess");

		m_FrameIndex = (m_FrameIndex + 1) % kMaxFramesInFlight;
	}

	void RenderManager::RetireAndReleasePacket(FramePacket& packet, uint64_t retireValue)
	{
		m_ResourceRegistry.RetireReleasedSlots(packet, retireValue);
		m_ResourceRegistry.FreeRetiredSlots(m_RenderDevice.GetCompletedTimelineValue(QueueType::Graphics), packet);

		// The main thread can write into it again
		m_FramePump.Release(packet);
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

	void RenderManager::RecreateResources(const glm::uvec2& surfaceExtent)
	{
		m_RenderDevice.WaitIdle();

		// The reported extent, not Window::GetSize(): Window is main thread only, and this will run on the render thread
		m_SwapChain.Recreate(surfaceExtent);
	}

}