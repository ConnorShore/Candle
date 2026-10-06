#pragma once

// Shared setup for the Vulkan-backed tests. Only test files under Tests/Render include Vulkan.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Render/RenderInstance.h>
#include <Candle/Render/RenderSpecification.h>

#include <string>
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

	// The debug messenger logs every Vulkan message as a Render-channel error. Flushes first, so call it
	// after the Vulkan objects under test are destroyed to include their teardown messages.
	inline std::vector<std::string> VulkanDebugMessages(LoggerFixture& logs)
	{
		CDL_CHECK(Logger::Flush());

		std::vector<std::string> messages;
		for (const LogRecord& record : logs.Records)
		{
			if (record.Channel == LogChannel::Render && record.Level >= LogLevel::Error)
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

}
