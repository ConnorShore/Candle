// RenderInstance against the real Vulkan loader: creation with and without validation, and the debug
// messenger's severity filter. The Gpu type, and it needs SDL video too: the instance asks the windowing
// backend for its surface extensions.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/RenderInstance.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	constexpr const char* kInjectedMessage = "Candle-Test injected message";

	// Sends a message down the instance's debug-utils path, the same route validation output takes.
	void SubmitDebugMessage(RenderInstance& instance, vk::DebugUtilsMessageSeverityFlagBitsEXT severity)
	{
		const vk::DebugUtilsMessengerCallbackDataEXT data{ .pMessage = kInjectedMessage };
		instance.GetVulkanInstance().submitDebugUtilsMessageEXT(severity, vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation, data);
	}

	bool AnyContains(const std::vector<std::string>& messages, std::string_view text)
	{
		return std::ranges::any_of(messages, [text](const std::string& message) { return message.find(text) != std::string::npos; });
	}

}

CDL_TEST_CASE(RenderInstance, CreatesWithValidation, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		WindowingBackend backend;
		RenderInstance instance(TestInstanceSpec(StrictRenderSpec()));
		CDL_EXPECT(*instance.GetVulkanInstance());
	}
	ExpectNoVulkanDebugMessages(logs);
}

CDL_TEST_CASE(RenderInstance, CreatesWithoutValidation, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	WindowingBackend backend;

	RenderSpecification spec;
	spec.ValidationSpec.EnableValidation = false;
	RenderInstance instance(TestInstanceSpec(spec));
	CDL_EXPECT(*instance.GetVulkanInstance());
}

CDL_TEST_CASE(RenderInstance, ForwardsMessagesAtTheConfiguredSeverity, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		WindowingBackend backend;
		RenderInstance instance(TestInstanceSpec(StrictRenderSpec()));
		SubmitDebugMessage(instance, vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning);
	}
	CDL_EXPECT(AnyContains(VulkanDebugMessages(logs), kInjectedMessage));
}

CDL_TEST_CASE(RenderInstance, DropsMessagesBelowTheConfiguredSeverity, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		WindowingBackend backend;
		RenderInstance instance(TestInstanceSpec(StrictRenderSpec()));
		SubmitDebugMessage(instance, vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo);
	}
	CDL_EXPECT_FALSE(AnyContains(VulkanDebugMessages(logs), kInjectedMessage));
}

// A validation error is a bug, so the spec a game gets without touching validation settings must report it.
CDL_TEST_CASE(RenderInstance, DefaultSpecForwardsValidationErrors, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		WindowingBackend backend;
		RenderInstance instance(TestInstanceSpec(RenderSpecification{}));
		SubmitDebugMessage(instance, vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
	}
	CDL_EXPECT(AnyContains(VulkanDebugMessages(logs), kInjectedMessage));
}
