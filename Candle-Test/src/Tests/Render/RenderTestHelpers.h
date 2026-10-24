#pragma once

// Shared setup for the Vulkan-backed tests. Only test files under Tests/Render include Vulkan.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Render/FramePacket.h>
#include <Candle/Render/GeometryPool.h>
#include <Candle/Render/GPUAllocator.h>
#include <Candle/Render/GPUBuffer.h>
#include <Candle/Render/GPUResourceRegistry.h>
#include <Candle/Render/GPUUploader.h>
#include <Candle/Render/RenderDevice.h>
#include <Candle/Render/RenderInstance.h>
#include <Candle/Render/RenderSpecification.h>
#include <Candle/Render/ShaderCreationInfo.h>
#include <Candle/Render/ShaderInterop/Vertex.h>

#include <vk_mem_alloc.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Candle::Test {

	// Validation on, reporting warnings and errors: a correct program produces neither. Set explicitly, so
	// the tests don't lean on the default, which DefaultSpecForwardsValidationErrors checks on its own.
	inline RenderSpecification StrictRenderSpec()
	{
		RenderSpecification spec;
		spec.ValidationSpec.EnableValidation = true;
		spec.ValidationSpec.Severity = ValidationSpecification::ValidationSeverity::Warning;
		return spec;
	}

	inline RenderInstanceSpecification TestInstanceSpec(const RenderSpecification& renderSpec)
	{
		return {
			.VulkanVersion = renderSpec.VulkanVersion,
			.ValidationSpec = renderSpec.ValidationSpec,
			.ApplicationName = "Candle-Test",
		};
	}

	// The debug messenger logs on the Render channel; pass to LoggerFixture to capture only that.
	inline constexpr uint16_t kRenderChannelMask = static_cast<uint16_t>(LogChannel::Render);

	// The debug messenger logs at the message's own severity, so match its prefix rather than a level: Render
	// also warns about non-Vulkan things. Flushes first, so call it after the Vulkan objects under test are
	// destroyed to include their teardown messages.
	inline std::vector<std::string> VulkanDebugMessages(LoggerFixture& logs)
	{
		CDL_CHECK(Logger::Flush());

		std::vector<std::string> messages;
		for (const LogRecord& record : logs.Records)
		{
			if (record.Channel == LogChannel::Render && std::string_view(record.Message).starts_with("[Vulkan Debug]"))
				messages.emplace_back(record.Message);
		}
		return messages;
	}

	// Notes every message, so a validation error names its VUID in the test output.
	inline void ExpectNoVulkanDebugMessages(LoggerFixture& logs)
	{
		const std::vector<std::string> messages = VulkanDebugMessages(logs);
		for (const std::string& message : messages)
			CDL_NOTE(message);
		CDL_EXPECT_EQ(messages.size(), 0u);
	}

	// Members construct in order and destroy in reverse, so the device goes before the instance and the backend.
	struct TestDevice
	{
		WindowingBackend Backend;
		RenderSpecification Spec = StrictRenderSpec();
		RenderInstance Instance{ TestInstanceSpec(Spec) };
		RenderDevice Device{ Instance, Spec };
	};

	// A device and the VMA allocator over it. The base is destroyed last, so the allocator goes before the device.
	struct TestAllocator : TestDevice
	{
		GPUAllocator Allocator{ { .VulkanVersion = Spec.VulkanVersion, .Device = &Device, .Instance = &Instance } };
	};

	// What GeometryPool needs, in dependency order: it stages through the uploader, which allocates through the allocator.
	// Uploads don't block, so the GPU is drained before the pool frees the meshes it still holds.
	struct TestGeometry : TestAllocator
	{
		GPUUploader Uploader{ Device, Allocator };
		GPUResourceRegistry Registry;
		GeometryPool Pool{ Allocator, Uploader, Registry };

		inline ~TestGeometry() { Device.WaitIdle(); }
	};

	// Stands in for one frame of RenderManager: pending releases ride a packet to an empty submit, retire with it once
	// the GPU reaches it, and their slots come back to the main thread. Frees the uploads' staging too.
	inline void RunReleaseFrame(TestGeometry& gpu)
	{
		FramePacket packet;
		gpu.Registry.MoveReleasedSlotsIntoPacket(packet);

		const uint64_t retireValue = gpu.Device.Submit(QueueType::Graphics, {});
		gpu.Registry.RetireReleasedSlots(packet, retireValue);
		gpu.Device.WaitFor(QueueType::Graphics, retireValue);
		gpu.Registry.FreeRetiredSlots(gpu.Device.GetCompletedTimelineValue(QueueType::Graphics), packet);

		gpu.Registry.ReturnRecycledSlotsToPools(packet);
		gpu.Uploader.FreeCompletedStagingBuffers();
	}

	// One triangle and its indices, for tests that need any mesh at all.
	inline const std::array<GPU::Vertex, 3> kTriangleVertices = { {
		{ { 0.5f, -0.5f, 0.0f } },
		{ { 0.0f, 0.5f, 0.0f } },
		{ { -0.5f, -0.5f, 0.0f } } } };
	inline constexpr std::array<uint32_t, 3> kTriangleIndices = { 0, 1, 2 };

	// Every allocation VMA holds right now, so a test can see a buffer freed, or leaked.
	inline uint32_t LiveAllocations(GPUAllocator& allocator)
	{
		VmaTotalStatistics stats{};
		vmaCalculateStatistics(allocator.GetAllocator(), &stats);
		return stats.total.statistics.allocationCount;
	}

	// Host-visible with random access, since the test reads it back; a transfer destination so the GPU can fill it.
	inline GPUBuffer CreateHostBuffer(GPUAllocator& allocator, vk::DeviceSize size)
	{
		return allocator.CreateBuffer({
			.m_Size = size,
			.m_Usage = vk::BufferUsageFlagBits::eTransferDst,
			.m_Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT });
	}

	// Invalidates first, in case the memory is not host-coherent.
	inline std::vector<std::byte> ReadBack(GPUAllocator& allocator, const GPUBuffer& buffer)
	{
		std::vector<std::byte> bytes(buffer.GetSize());
		CDL_CHECK(vmaCopyAllocationToMemory(allocator.GetAllocator(), buffer.GetAllocation(), 0, bytes.data(), bytes.size()) == VK_SUCCESS);
		return bytes;
	}

	// Distinct, recognisable bytes: seed, seed + 1, ... wrapping at 256.
	inline std::vector<std::byte> BytePattern(size_t count, uint8_t seed)
	{
		std::vector<std::byte> bytes(count);
		for (size_t i = 0; i < count; ++i)
			bytes[i] = static_cast<std::byte>(static_cast<uint8_t>(seed + i));
		return bytes;
	}

	// One member of an interop struct as C++ lays it out, to compare against a shader's reflection.
	struct InteropMember
	{
		std::string_view Name;
		uint32_t Offset;
		uint32_t Size;
	};
#define CDL_INTEROP_MEMBER(Type, Member) \
	::Candle::Test::InteropMember{ #Member, static_cast<uint32_t>(offsetof(Type, Member)), static_cast<uint32_t>(sizeof(Type::Member)) }

	// Fails unless the reflected block holds exactly these members, at the offsets and sizes C++ gives them.
	inline void ExpectPushBlockMatches(const PushConstantBlock& block, size_t cppSize, std::span<const InteropMember> members)
	{
		CDL_EXPECT_EQ(block.Size, cppSize);
		CDL_CHECK_EQ(block.Members.size(), members.size());
		for (size_t i = 0; i < members.size(); ++i)
		{
			const PushConstantMember& reflected = block.Members[i];
			CDL_EXPECT_MSG(reflected.Name == members[i].Name, std::format("member {} is '{}', expected '{}'", i, reflected.Name, members[i].Name));
			CDL_EXPECT_MSG(reflected.Offset == members[i].Offset,
				std::format("{} is at {} in the shader, {} in C++", members[i].Name, reflected.Offset, members[i].Offset));
			CDL_EXPECT_MSG(reflected.Size == members[i].Size,
				std::format("{} is {} bytes in the shader, {} in C++", members[i].Name, reflected.Size, members[i].Size));
		}
	}

	// One pool and primary buffer on the graphics family, for tests that record their own commands.
	// Reset the pool only once the GPU has finished with the buffer.
	struct TestCommands
	{
		vk::raii::CommandPool Pool{ nullptr };
		vk::raii::CommandBuffer Buffer{ nullptr };

		explicit TestCommands(RenderDevice& device)
		{
			vk::CommandPoolCreateInfo poolInfo{ .queueFamilyIndex = device.GetQueueFamily(QueueType::Graphics) };
			Pool = vk::raii::CommandPool(device.GetDevice(), poolInfo);

			vk::CommandBufferAllocateInfo allocInfo{ .commandPool = *Pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
			Buffer = std::move(vk::raii::CommandBuffers(device.GetDevice(), allocInfo).front());
		}
	};

	// Submits one command buffer on the graphics queue and blocks until the GPU has run it.
	inline void SubmitAndWait(RenderDevice& device, const vk::raii::CommandBuffer& commandBuffer)
	{
		const vk::CommandBufferSubmitInfo commandBufferInfo{ .commandBuffer = *commandBuffer };
		device.WaitFor(QueueType::Graphics, device.Submit(QueueType::Graphics, { .CommandBuffers = { &commandBufferInfo, 1 } }));
	}

	// Compiled shaders are build outputs, not checked-in files, so a missing one is a setup step rather than a failure.
	inline void SkipUnlessCompiled(const std::filesystem::path& spirvPath)
	{
		const auto file = Platform::ReadFile(spirvPath);
		if (!file && file.error().Code == FileErrorCode::NotFound)
			CDL_SKIP(std::format("{} is not compiled; run Build.py shaders", spirvPath.string()));
	}

}
