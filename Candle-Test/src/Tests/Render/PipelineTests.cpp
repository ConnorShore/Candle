// Pipeline on a real device, from a fixture Build.py shaders compiles out of Candle-Test/res/shaders.
// Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/Pipeline.h>
#include <Candle/Render/PipelineSpecification.h>
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

	// The spec requires every implementation to support it as a blendable color attachment.
	constexpr vk::Format kColorFormat = vk::Format::eR8G8B8A8Unorm;

}

// Every blend mode Pipeline implements, with and without culling. DstAlpha and OneMinusDstAlpha hit its
// unknown-blend-mode assert, so they are left out until they exist.
CDL_TEST_CASE(Pipeline, CreatesForEachBlendModeWithoutValidationMessages, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		SharedPtr<Shader> shader = ShaderLoader::LoadShader("PipelineFixture", kFixturePath, gpu.Device);

		for (BlendMode blend : { BlendMode::None, BlendMode::SrcAlpha, BlendMode::OneMinusSrcAlpha })
		{
			for (bool cullBackFaces : { true, false })
			{
				PipelineSpecification spec{
					.Shader = shader,
					.Blend = blend,
					.ColorAttachmentFormats = { kColorFormat },
					.CullBackFaces = cullBackFaces };
				Pipeline pipeline(spec, gpu.Device, kColorFormat);

				const std::string label = std::format("blend {}, cull {}", std::to_underlying(blend), cullBackFaces);
				CDL_EXPECT_MSG(*pipeline.GetPipeline(), label);
				CDL_EXPECT_MSG(*pipeline.GetLayout(), label);
			}
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}
