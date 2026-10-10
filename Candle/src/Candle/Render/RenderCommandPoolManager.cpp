#include "cdlpch.h"
#include "RenderCommandPoolManager.h"
#include "RenderDevice.h"

namespace Candle {

#pragma region ThreadFrameCommands Impl

	ThreadFrameCommands::ThreadFrameCommands(RenderDevice& renderDevice, uint32_t queueFamilyIndex)
		: m_RenderDevice(renderDevice)
	{
		vk::CommandPoolCreateInfo poolInfo{ 
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = queueFamilyIndex 
		};

		m_CommandPool = vk::raii::CommandPool(m_RenderDevice.GetDevice(), poolInfo);
	}

	vk::raii::CommandBuffer& ThreadFrameCommands::AquireCommandBuffer()
	{
		vk::CommandBufferAllocateInfo allocInfo{
			.commandPool = m_CommandPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1
		};

		m_PrimaryCommandBuffers.emplace_back(std::move(vk::raii::CommandBuffers(m_RenderDevice.GetDevice(), allocInfo).front()));
		return m_PrimaryCommandBuffers[m_PrimaryCommandBuffersUsed++];
	}

	void ThreadFrameCommands::Reset()
	{
		m_CommandPool.reset(vk::CommandPoolResetFlagBits::eReleaseResources);
		m_PrimaryCommandBuffers.clear();
		m_PrimaryCommandBuffersUsed = 0;
	}

#pragma endregion

#pragma region RenderCommandPoolManager Impl

	RenderCommandPoolManager::RenderCommandPoolManager(RenderDevice& renderDevice, uint32_t threadCount)
	{
		for (uint32_t frameIndex = 0; frameIndex < kMaxFramesInFlight; ++frameIndex)
		{
			m_GraphicsCommands[frameIndex].reserve(threadCount);

			for (uint32_t threadIndex = 0; threadIndex < threadCount; ++threadIndex)
			{
				m_GraphicsCommands[frameIndex].emplace_back(renderDevice, renderDevice.GetQueueFamily(QueueType::Graphics));
			}
		}
	}

	void RenderCommandPoolManager::BeginFrame(uint32_t frameIndex)
	{
		for (auto& threadCommands : m_GraphicsCommands[frameIndex])
			threadCommands.Reset();
	}

#pragma endregion

}