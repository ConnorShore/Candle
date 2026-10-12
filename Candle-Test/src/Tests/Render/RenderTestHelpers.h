#pragma once

// Shared setup for the Vulkan-backed tests. Only test files under Tests/Render include Vulkan.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Render/RenderDevice.h>
#include <Candle/Render/RenderInstance.h>
#include <Candle/Render/RenderSpecification.h>

#include <filesystem>
#include <format>
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
