// TrianglePipeline building the engine's triangle from Candle/res/shaders/bin/Triangle.spv, a build output.
// Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Render/BindlessTable.h>
#include <Candle/Render/Pipeline.h>
#include <Candle/Render/PipelineSpecification.h>
#include <Candle/Render/Pipelines/TriangleInterop.h>
#include <Candle/Render/Pipelines/TrianglePipeline.h>
#include <Candle/Render/Shader.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	const std::filesystem::path kTriangleShaderPath = "Candle/res/shaders/bin/Triangle.spv";

	constexpr vk::Format kColorFormat = vk::Format::eR8G8B8A8Unorm;

	// Every device must support it as an attachment, and it isn't the D32 Renderer usually picks, so a hardcoded format shows
	constexpr vk::Format kDepthFormat = vk::Format::eD16Unorm;

	const ShaderEntryPoint* FindEntryPoint(const Shader& shader, std::string_view name)
	{
		const auto& entryPoints = shader.GetEntryPoints();
		const auto it = std::ranges::find(entryPoints, name, &ShaderEntryPoint::Name);
		return it == entryPoints.end() ? nullptr : &*it;
	}

}

CDL_TEST_CASE(TrianglePipeline, DescribesTheTriangleShaderAndTarget, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const TrianglePipeline triangle;
		const PipelineSpecification spec = triangle.CreateSpecification(gpu.Device, kColorFormat, kDepthFormat);

		CDL_EXPECT_EQ(triangle.GetName(), std::string("BasicTriangle"));
		CDL_CHECK(spec.Shader);
		CDL_EXPECT_EQ(spec.Shader->GetName(), triangle.GetName());
		CDL_EXPECT(spec.Shader->GetFilePath() == kTriangleShaderPath);	// Repo-relative, so any clone finds it
		CDL_EXPECT_EQ(spec.Topology, PrimitiveTopology::TriangleList);
		CDL_EXPECT(spec.ColorAttachmentFormats == std::vector<vk::Format>{ kColorFormat });
		CDL_EXPECT(spec.DepthAttachmentFormat == kDepthFormat);

		const ShaderEntryPoint* vertex = FindEntryPoint(*spec.Shader, "VertMain");
		const ShaderEntryPoint* fragment = FindEntryPoint(*spec.Shader, "FragMain");
		CDL_CHECK(vertex && fragment);
		CDL_EXPECT_EQ(vertex->Stage, ShaderStage::Vertex);
		CDL_EXPECT_EQ(fragment->Stage, ShaderStage::Fragment);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The fragment shader samples set 0, so validation also checks each binding it declares against BindlessTable's layout.
CDL_TEST_CASE(TrianglePipeline, BuildsAPipelineWithoutValidationMessages, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		Pipeline pipeline(TrianglePipeline{}.CreateSpecification(gpu.Device, kColorFormat, kDepthFormat), gpu.Device, bindless);

		CDL_EXPECT(*pipeline.GetPipeline());
		CDL_EXPECT(*pipeline.GetLayout());
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The Renderer pushes GPU::MeshDrawPushConstants into this pipeline, so the compiled shader must agree with it byte for byte.
CDL_TEST_CASE(TrianglePipeline, ShaderPushBlockMatchesMeshDrawPushConstants, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const PipelineSpecification spec = TrianglePipeline{}.CreateSpecification(gpu.Device, kColorFormat, kDepthFormat);

		using PushConstants = GPU::MeshDrawPushConstants;
		const std::array members = {
			CDL_INTEROP_MEMBER(PushConstants, Global),
			CDL_INTEROP_MEMBER(PushConstants, Vertices),
			CDL_INTEROP_MEMBER(PushConstants, TextureIndex),
			CDL_INTEROP_MEMBER(PushConstants, SamplerIndex),
			CDL_INTEROP_MEMBER(PushConstants, TransformIndex) };
		for (const ShaderEntryPoint& entryPoint : spec.Shader->GetEntryPoints())
		{
			CDL_NOTE(entryPoint.Name);
			ExpectPushBlockMatches<PushConstants>(entryPoint.PushConstants, members);
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}
