#include "cdlpch.h"
#include "RenderManager.h"
#include "Candle/Render/Pipelines/ShaderLoader.h"
#include "Candle/Render/Pipelines/Shader.h"
#include "Candle/Render/FramePacket.h"
#include "Candle/Render/RenderConstants.h"

#include "Candle/Platform/Window.h"
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

	RenderManager::RenderManager(const RenderSpecification& renderSpec, const ApplicationInfo& appInfo, const std::filesystem::path& engineAssetDir, Window& window, JobSystem& jobSystem)
		: m_Specification(renderSpec)
		, m_RenderInstance(CreateRenderInstanceSpec(renderSpec, appInfo))
		, m_RenderDevice(m_RenderInstance, m_Specification)
		, m_SwapChain(m_RenderInstance, m_RenderDevice, window, renderSpec.EnableVSync)
		, m_GPUAllocator({ renderSpec.VulkanVersion, &m_RenderDevice, &m_RenderInstance })
		, m_GPUUploader(m_RenderDevice, m_GPUAllocator)
		, m_BindlessTable(m_RenderDevice)
		, m_GeometryPool(m_GPUAllocator, m_GPUUploader, m_ResourceRegistry)
		, m_TexturePool(m_GPUAllocator, m_GPUUploader, m_ResourceRegistry, m_BindlessTable)
		, m_Renderer(m_RenderDevice, m_GeometryPool, m_TexturePool, m_BindlessTable, m_GPUAllocator, {
			.TargetFormat = m_SwapChain.GetSurfaceFormat().format,
			.TargetExtent = m_SwapChain.GetExtent(),
			.ShaderDirectory = engineAssetDir / "shaders/bin" })
		, m_CommandPoolManager(m_RenderDevice, jobSystem.GetNumWorkers())
		, m_RenderThread({ .Name = "Candle-RenderThread", .Priority = ThreadPriority::High })
	{
		CreateFrameContexts();
	}

	RenderManager::~RenderManager()
	{
		JoinRenderThread();

		// Members are destroyed after this body returns, and the GPU may still be using them
		m_RenderDevice.WaitIdle();
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

		m_RenderThread.Join();
		CDL_CORE_INFO(LogChannel::Render, "Render Thread Joined");

		// After the join, so no frame can be submitted once the wait returns
		m_RenderDevice.WaitIdle();
	}

	void RenderManager::RegisterAssetHandlers(AssetManager& assetManager)
	{
		assetManager.RegisterAssetHandler(ScopedPtr<MeshAssetHandler>::Create(m_GeometryPool));
		assetManager.RegisterAssetHandler(ScopedPtr<TextureHandler>::Create(m_TexturePool));
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
			// Error codes already threw inside acquireNextImage, and eTimeout or eNotReady can't happen with no timeout
			CDL_CORE_PANIC(LogChannel::Render, "Failed to acquire swap chain image: {}", vk::to_string(result));
		}

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

		for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
		{
			FrameContext& frameContext = m_FrameContexts[i];
			frameContext.CommandPool = vk::raii::CommandPool(device, poolInfo);
			m_RenderDevice.SetDebugName(*frameContext.CommandPool, std::format("Command Pool [frame {}]", i));

			vk::CommandBufferAllocateInfo allocInfo{
				.commandPool = *frameContext.CommandPool,
				.level = vk::CommandBufferLevel::ePrimary,
				.commandBufferCount = 1
			};
			frameContext.CommandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
			m_RenderDevice.SetDebugName(*frameContext.CommandBuffer, std::format("Command Buffer [frame {}]", i));

			// Written by the CPU every frame and read once per draw, so host-visible rather than staged into device memory
			frameContext.FrameData = m_GPUAllocator.CreateBuffer({
				.Size = sizeof(GPU::FrameData),
				.Usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
				.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				.DebugName = std::format("FrameData [frame {}]", i) });

			frameContext.TransformData = m_GPUAllocator.CreateBuffer({
				.Size = kMaxTransforms * sizeof(glm::mat4),
				.Usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
				.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				.DebugName = std::format("TransformData [frame {}]", i) });
		}
	}

	void RenderManager::RecreateResources(const glm::uvec2& surfaceExtent)
	{
		// The one idle outside shutdown: resizes are rare, and the old swapchain's presents have no fence to wait on instead
		m_RenderDevice.WaitIdle();

		// The reported extent, not Window::GetSize(): Window is main thread only, and this will run on the render thread
		m_SwapChain.Recreate(surfaceExtent);

		// The swapchain's real extent, not surfaceExtent: the surface may override the request, and a stale packet's size is one
		m_Renderer.RecreateTargets(m_SwapChain.GetExtent(), m_GPUAllocator);
	}

}