// Pipeline on a real device, from a fixture Build.py shaders compiles out of Candle-Test/res/shaders.
// Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"
#include "ShaderInteropFixture.h"

#include <Candle/Render/BindlessTable.h>
#include <Candle/Render/Pipeline.h>
#include <Candle/Render/PipelineSpecification.h>
#include <Candle/Render/Pipelines/TriangleInterop.h>
#include <Candle/Render/RenderDevice.h>
#include <Candle/Render/ShaderLoader.h>

#include <filesystem>
#include <format>
#include <string>
#include <type_traits>
#include <utility>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

// A copy would be a second owner destroying the same VkPipeline.
CDL_STATIC_ASSERT(!std::is_copy_constructible_v<Pipeline>);
CDL_STATIC_ASSERT(!std::is_copy_assignable_v<Pipeline>);
CDL_STATIC_ASSERT(std::is_move_constructible_v<Pipeline>);
CDL_STATIC_ASSERT(std::is_move_assignable_v<Pipeline>);

namespace {

	// Relative to the repo root, which is where Build.py test and the Visual Studio debugger both run from.
	const std::filesystem::path kFixturePath = "Candle-Test/res/shaders/bin/PipelineFixture.spv";

	// A vertex and fragment pair whose push block follows the engine contract (see ShaderInteropFixture.h).
	const std::filesystem::path kInteropFixturePath = "Candle-Test/res/shaders/bin/ShaderInteropFixture.spv";

	// The spec requires every implementation to support it as a blendable color attachment.
	constexpr vk::Format kColorFormat = vk::Format::eR8G8B8A8Unorm;

	// Every layout includes the bindless set, so a pipeline needs the table even when its shader samples nothing.
	Pipeline CreatePipeline(RenderDevice& device, BindlessTable& bindless, const std::filesystem::path& shaderPath)
	{
		PipelineSpecification spec{
			.Shader = LoadTestShader(shaderPath.stem().string(), shaderPath),
			.ColorAttachmentFormats = { kColorFormat } };
		return Pipeline(spec, device, bindless);
	}

}

// 128 bytes is the spec's guaranteed minimum, so every device must accept the engine-wide range.
CDL_TEST_CASE(Pipeline, SharedPushConstantRangeFitsTheDevice, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		CDL_EXPECT_LE(kPushConstantsSize, gpu.Device.GetPhysicalDevice().getProperties().limits.maxPushConstantsSize);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Validation checks the layout's range covers the shader's block in each stage using it. The fixture's C layout
// offsets are invalid std430, so this also fails if the device stops enabling scalarBlockLayout.
CDL_TEST_CASE(Pipeline, CreatesFromAShaderFollowingThePushConstantContract, Gpu)
{
	SkipUnlessCompiled(kInteropFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		Pipeline pipeline = CreatePipeline(gpu.Device, bindless, kInteropFixturePath);

		CDL_EXPECT(*pipeline.GetPipeline());
		CDL_EXPECT(*pipeline.GetLayout());
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Every layout declares the same range, so a pipeline whose shader has no push constants still takes a push.
// Validation checks each against the layout's range and stages as it is recorded.
CDL_TEST_CASE(Pipeline, PushConstantsRecordAgainstTheSharedRange, Gpu)
{
	SkipUnlessCompiled(kFixturePath);
	SkipUnlessCompiled(kInteropFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		Pipeline withPushBlock = CreatePipeline(gpu.Device, bindless, kInteropFixturePath);
		Pipeline withoutPushBlock = CreatePipeline(gpu.Device, bindless, kFixturePath);
		TestCommands commands(gpu.Device);

		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		withPushBlock.PushConstants(*commands.Buffer, GPU::InteropFixturePushConstants{});
		withoutPushBlock.PushConstants(*commands.Buffer, GPU::MeshDrawPushConstants{});
		commands.Buffer.end();
		SubmitAndWait(gpu.Device, commands.Buffer);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Every blend mode, with and without culling.
CDL_TEST_CASE(Pipeline, CreatesForEachBlendModeWithoutValidationMessages, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		SharedPtr<Shader> shader = LoadTestShader("PipelineFixture", kFixturePath);

		for (BlendMode blend : { BlendMode::Opaque, BlendMode::AlphaBlend, BlendMode::PremultipliedAlpha, BlendMode::Additive })
		{
			for (bool cullBackFaces : { true, false })
			{
				PipelineSpecification spec{
					.Shader = shader,
					.Blend = blend,
					.ColorAttachmentFormats = { kColorFormat },
					.CullBackFaces = cullBackFaces };
				Pipeline pipeline(spec, gpu.Device, bindless);

				const std::string label = std::format("blend {}, cull {}", std::to_underlying(blend), cullBackFaces);
				CDL_EXPECT_MSG(*pipeline.GetPipeline(), label);
				CDL_EXPECT_MSG(*pipeline.GetLayout(), label);
			}
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}
