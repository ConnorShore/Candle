// DebugDrawPipeline building the engine's debug draw pipeline from its shader under the engine asset directory, a build output.
// Each GPU test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Render/Pipelines/DebugDrawInterop.h>
#include <Candle/Render/Pipelines/DebugDrawPipeline.h>
#include <Candle/Render/Pipelines/Pipeline.h>
#include <Candle/Render/Pipelines/PipelineSpecification.h>
#include <Candle/Render/Pipelines/Shader.h>
#include <Candle/Render/Vulkan/BindlessTable.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;
using Candle::Test::Type::Gpu;

namespace {

	constexpr vk::Format kColorFormat = vk::Format::eR8G8B8A8Unorm;
	constexpr vk::Format kDepthFormat = vk::Format::eD16Unorm;

	const ShaderEntryPoint* FindEntryPoint(const Shader& shader, std::string_view name)
	{
		const auto& entryPoints = shader.GetEntryPoints();
		const auto it = std::ranges::find(entryPoints, name, &ShaderEntryPoint::Name);
		return it == entryPoints.end() ? nullptr : &*it;
	}

}

// Lines rather than triangles, and it keeps the depth format: the world buckets depth test against the scene's depth
CDL_TEST_CASE(DebugDrawPipeline, DescribesALineListPipelineWithDepth, Gpu)
{
	SkipUnlessCompiled(kDebugDrawShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const DebugDrawPipeline debugDraw(kEngineShaderDirectory);
		const PipelineSpecification spec = debugDraw.CreateSpecification(gpu.Device, kColorFormat, kDepthFormat);

		CDL_EXPECT_EQ(debugDraw.GetName(), std::string("DebugDraw"));
		CDL_CHECK(spec.Shader);
		CDL_EXPECT_EQ(spec.Shader->GetName(), debugDraw.GetName());
		CDL_EXPECT(spec.Shader->GetFilePath() == kDebugDrawShaderPath);
		CDL_EXPECT_EQ(spec.DebugName, debugDraw.GetName());
		CDL_EXPECT_EQ(spec.Topology, PrimitiveTopology::LineList);
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

// Windows opens the shader whatever case its name is in, but other platforms don't (see the GuiPipeline test of the same name)
CDL_TEST_CASE(DebugDrawPipeline, CompiledShaderIsNamedExactlyAsItIsLoaded, Unit)
{
	SkipUnlessCompiled(kDebugDrawShaderPath);

	const std::string loaded = kDebugDrawShaderPath.filename().string();
	std::string onDisk;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(kDebugDrawShaderPath.parent_path()))
	{
		const std::string name = entry.path().filename().string();
		if (std::ranges::equal(name, loaded, [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }))
			onDisk = name;
	}

	CDL_EXPECT_MSG(onDisk == loaded, std::format("DebugDrawPipeline loads {}, but the file is {}; rebuild the shaders from a clean bin", loaded, onDisk));
}

CDL_TEST_CASE(DebugDrawPipeline, BuildsAPipelineWithoutValidationMessages, Gpu)
{
	SkipUnlessCompiled(kDebugDrawShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		Pipeline pipeline(DebugDrawPipeline{ kEngineShaderDirectory }.CreateSpecification(gpu.Device, kColorFormat, kDepthFormat), gpu.Device, bindless);

		CDL_EXPECT(*pipeline.GetPipeline());
		CDL_EXPECT(*pipeline.GetLayout());
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The Renderer pushes GPU::DebugDrawPushConstants into this pipeline, so the compiled shader must agree with it byte for byte
CDL_TEST_CASE(DebugDrawPipeline, ShaderPushBlockMatchesDebugDrawPushConstants, Gpu)
{
	SkipUnlessCompiled(kDebugDrawShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const PipelineSpecification spec = DebugDrawPipeline{ kEngineShaderDirectory }.CreateSpecification(gpu.Device, kColorFormat, kDepthFormat);

		using PushConstants = GPU::DebugDrawPushConstants;
		const std::array members = {
			CDL_INTEROP_MEMBER(PushConstants, Global),
			CDL_INTEROP_MEMBER(PushConstants, Vertices) };
		for (const ShaderEntryPoint& entryPoint : spec.Shader->GetEntryPoints())
		{
			// FragMain never touches the block, so the compiler is free to drop it there
			if (entryPoint.PushConstants.Members.empty())
				continue;

			CDL_NOTE(entryPoint.Name);
			ExpectPushBlockMatches<PushConstants>(entryPoint.PushConstants, members);
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}
