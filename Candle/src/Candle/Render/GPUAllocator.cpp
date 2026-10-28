#include "cdlpch.h"
#include "GPUAllocator.h"
#include "RenderDevice.h"
#include "RenderInstance.h"
#include "VulkanUtils.h"

namespace Candle {

	GPUAllocator::GPUAllocator(GPUAllocatorSpec spec)
		: m_Spec(spec)
	{
		VmaAllocatorCreateInfo allocatorCreateInfo = {};
		allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		if (m_Spec.Device->HasMemoryBudget())
			allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;	// VMA trusts this flag, so it must match the device
		allocatorCreateInfo.vulkanApiVersion = ToVulkanApiVersion(m_Spec.VulkanVersion);
		allocatorCreateInfo.physicalDevice = static_cast<VkPhysicalDevice>(*m_Spec.Device->GetPhysicalDevice());
		allocatorCreateInfo.device = static_cast<VkDevice>(*m_Spec.Device->GetDevice());
		allocatorCreateInfo.instance = static_cast<VkInstance>(*m_Spec.Instance->GetVulkanInstance());

		vmaCreateAllocator(&allocatorCreateInfo, &m_Allocator);
	}

	GPUAllocator::~GPUAllocator()
	{
		vmaDestroyAllocator(m_Allocator);
	}

	GPUBuffer GPUAllocator::CreateBuffer(GPUBufferSpec spec)
	{
		return { spec, *this };
	}

	GPUImage GPUAllocator::CreateImage(GPUImageSpec spec)
	{
		return { spec, *this };
	}
}