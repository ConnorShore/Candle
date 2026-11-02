// GuiPipeline building the engine's GUI pipeline from the GUI shader under the engine asset directory, a build output.
// Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../../RenderTestHelpers.h"

#include <Candle/Render/GUI/GuiInterop.h>
#include <Candle/Render/Vulkan/BindlessTable.h>
#include <Candle/Render/Pipelines/GUI/GuiPipeline.h>
#include <Candle/Render/Pipelines/Pipeline.h>
#include <Candle/Render/Pipelines/PipelineSpecification.h>
#include <Candle/Render/Pipelines/Shader.h>

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

	// Renderer offers every pipeline its depth format; the GUI pass has no depth attachment, so this one must ignore it
	constexpr vk::Format kDepthFormat = vk::Format::eD16Unorm;

	const ShaderEntryPoint* FindEntryPoint(const Shader& shader, std::string_view name)
	{
		const auto& entryPoints = shader.GetEntryPoints();
		const auto it = std::ranges::find(entryPoints, name, &ShaderEntryPoint::Name);
		return it == entryPoints.end() ? nullptr : &*it;
	}

}

// Alpha-blended, since ImGui's alpha is coverage; unculled, since ImGui doesn't keep one winding; and colour only, since
// the GUI pass renders with no depth attachment and a pipeline expecting one fails validation there.
CDL_TEST_CASE(GuiPipeline, DescribesAnAlphaBlendedColourOnlyPipeline, Gpu)
{
	SkipUnlessCompiled(kGuiShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const GuiPipeline gui(kEngineShaderDirectory);
		const PipelineSpecification spec = gui.CreateSpecification(gpu.Device, kColorFormat, kDepthFormat);

		CDL_EXPECT_EQ(gui.GetName(), std::string("Gui"));
		CDL_CHECK(spec.Shader);
		CDL_EXPECT_EQ(spec.Shader->GetName(), gui.GetName());
		CDL_EXPECT(spec.Shader->GetFilePath() == kGuiShaderPath);
		CDL_EXPECT_EQ(spec.DebugName, gui.GetName());
		CDL_EXPECT_EQ(spec.Topology, PrimitiveTopology::TriangleList);
		CDL_EXPECT_EQ(spec.Blend, BlendMode::AlphaBlend);
		CDL_EXPECT_FALSE(spec.CullBackFaces);
		CDL_EXPECT(spec.ColorAttachmentFormats == std::vector<vk::Format>{ kColorFormat });
		CDL_EXPECT(spec.DepthAttachmentFormat == vk::Format::eUndefined);

		const ShaderEntryPoint* vertex = FindEntryPoint(*spec.Shader, "VertMain");
		const ShaderEntryPoint* fragment = FindEntryPoint(*spec.Shader, "FragMain");
		CDL_CHECK(vertex && fragment);
		CDL_EXPECT_EQ(vertex->Stage, ShaderStage::Vertex);
		CDL_EXPECT_EQ(fragment->Stage, ShaderStage::Fragment);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Windows opens the shader whatever case its name is in, but other platforms don't, so the compiled file must be named
// exactly as GuiPipeline loads it (the test above ties kGuiShaderPath to that). Read off the directory listing, which keeps
// the case the file really has.
CDL_TEST_CASE(GuiPipeline, CompiledShaderIsNamedExactlyAsItIsLoaded, Unit)
{
	SkipUnlessCompiled(kGuiShaderPath);

	const std::string loaded = kGuiShaderPath.filename().string();
	std::string onDisk;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(kGuiShaderPath.parent_path()))
	{
		const std::string name = entry.path().filename().string();
		if (std::ranges::equal(name, loaded, [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }))
			onDisk = name;
	}

	// A file compiled before a rename keeps its old case when overwritten, so a clean shader build is the fix there
	CDL_EXPECT_MSG(onDisk == loaded, std::format("GuiPipeline loads {}, but the file is {}; rebuild the shaders from a clean bin", loaded, onDisk));
}

// The fragment shader samples set 0, so validation also checks each binding it declares against BindlessTable's layout.
CDL_TEST_CASE(GuiPipeline, BuildsAPipelineWithoutValidationMessages, Gpu)
{
	SkipUnlessCompiled(kGuiShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		Pipeline pipeline(GuiPipeline{ kEngineShaderDirectory }.CreateSpecification(gpu.Device, kColorFormat, kDepthFormat), gpu.Device, bindless);

		CDL_EXPECT(*pipeline.GetPipeline());
		CDL_EXPECT(*pipeline.GetLayout());
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The Renderer pushes GPU::GuiDrawPushConstants into this pipeline, so the compiled shader must agree with it byte for byte.
CDL_TEST_CASE(GuiPipeline, ShaderPushBlockMatchesGuiDrawPushConstants, Gpu)
{
	SkipUnlessCompiled(kGuiShaderPath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const PipelineSpecification spec = GuiPipeline{ kEngineShaderDirectory }.CreateSpecification(gpu.Device, kColorFormat, kDepthFormat);

		using PushConstants = GPU::GuiDrawPushConstants;
		const std::array members = {
			CDL_INTEROP_MEMBER(PushConstants, Global),
			CDL_INTEROP_MEMBER(PushConstants, Vertices),
			CDL_INTEROP_MEMBER(PushConstants, Scale),
			CDL_INTEROP_MEMBER(PushConstants, Translate),
			CDL_INTEROP_MEMBER(PushConstants, TextureIndex),
			CDL_INTEROP_MEMBER(PushConstants, SamplerIndex) };
		for (const ShaderEntryPoint& entryPoint : spec.Shader->GetEntryPoints())
		{
			CDL_NOTE(entryPoint.Name);
			ExpectPushBlockMatches<PushConstants>(entryPoint.PushConstants, members);
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}
