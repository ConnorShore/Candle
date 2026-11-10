// The engine's own shaders under the engine asset directory, build outputs the Renderer builds its pipelines from: the file
// names it loads, the entry points it names and the push-constant structs it pushes into each. Reflection never touches a
// device, so these run headless.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Render/GUI/GuiInterop.h>
#include <Candle/Render/Pipelines/DebugDrawInterop.h>
#include <Candle/Render/Pipelines/TriangleInterop.h>
#include <Candle/Render/Pipelines/Shader.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	struct NamedEntryPoint
	{
		std::string_view Name;
		ShaderStage Stage;
		std::string_view StageName;
	};

	// The names the Renderer builds every one of its pipelines from
	constexpr NamedEntryPoint kRendererEntryPoints[] = {
		{ "VertMain", ShaderStage::Vertex, "vertex" },
		{ "FragMain", ShaderStage::Fragment, "fragment" } };

}

// A shader missing one fails CreatePipeline, and the Renderer only finds out mid-frame when the pipeline isn't there. PipelineStore
// takes each entry point's stage from reflection, so one named for the wrong stage would build a pipeline with two of a stage.
CDL_TEST_CASE(EngineShaders, EachHasTheEntryPointsTheRendererNames, Unit)
{
	SkipUnlessRendererShadersCompiled();

	for (const std::filesystem::path& shaderPath : { kTriangleShaderPath, kGuiShaderPath, kDebugDrawShaderPath })
	{
		const SharedPtr<Shader> shader = LoadTestShader(shaderPath.stem().string(), shaderPath);
		const std::vector<ShaderEntryPoint>& entryPoints = shader->GetEntryPoints();
		for (const NamedEntryPoint& expected : kRendererEntryPoints)
		{
			const auto it = std::ranges::find(entryPoints, expected.Name, &ShaderEntryPoint::Name);
			CDL_EXPECT_MSG(it != entryPoints.end() && it->Stage == expected.Stage,
				std::format("{} has no {} entry point named {}", shaderPath.filename().string(), expected.StageName, expected.Name));
		}
	}
}

// Windows opens a shader whatever case its name is in, but other platforms don't, so each compiled file must be named exactly
// as the Renderer loads it. Read off the directory listing, which keeps the case the file really has.
CDL_TEST_CASE(EngineShaders, CompiledShadersAreNamedExactlyAsTheyAreLoaded, Unit)
{
	SkipUnlessRendererShadersCompiled();

	for (const std::filesystem::path& shaderPath : { kTriangleShaderPath, kGuiShaderPath, kDebugDrawShaderPath })
	{
		const std::string loaded = shaderPath.filename().string();
		std::string onDisk;
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(shaderPath.parent_path()))
		{
			const std::string name = entry.path().filename().string();
			if (std::ranges::equal(name, loaded, [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }))
				onDisk = name;
		}

		// A file compiled before a rename keeps its old case when overwritten, so a clean shader build is the fix there
		CDL_EXPECT_MSG(onDisk == loaded, std::format("the Renderer loads {}, but the file is {}; rebuild the shaders from a clean bin", loaded, onDisk));
	}
}

// The Renderer pushes GPU::MeshDrawPushConstants into the triangle pipeline and GPU::ComputePushConstants into the test compute
// pipeline built from the same file, so each stage's block must agree with its struct byte for byte.
CDL_TEST_CASE(EngineShaders, TrianglePushBlocksMatchTheirPushConstants, Unit)
{
	SkipUnlessCompiled(kTriangleShaderPath);

	const SharedPtr<Shader> shader = LoadTestShader("Triangle", kTriangleShaderPath);

	using MeshDraw = GPU::MeshDrawPushConstants;
	const std::array meshDrawMembers = {
		CDL_INTEROP_MEMBER(MeshDraw, Global),
		CDL_INTEROP_MEMBER(MeshDraw, Vertices),
		CDL_INTEROP_MEMBER(MeshDraw, TextureIndex),
		CDL_INTEROP_MEMBER(MeshDraw, SamplerIndex),
		CDL_INTEROP_MEMBER(MeshDraw, TransformIndex),
		CDL_INTEROP_MEMBER(MeshDraw, ComputeTextureIndex) };

	using Compute = GPU::ComputePushConstants;
	const std::array computeMembers = {
		CDL_INTEROP_MEMBER(Compute, Global),
		CDL_INTEROP_MEMBER(Compute, TextureIndex),
		CDL_INTEROP_MEMBER(Compute, SamplerIndex) };

	for (const ShaderEntryPoint& entryPoint : shader->GetEntryPoints())
	{
		CDL_NOTE(entryPoint.Name);
		if (entryPoint.Stage == ShaderStage::Compute)
			ExpectPushBlockMatches<Compute>(entryPoint.PushConstants, computeMembers);
		else
			ExpectPushBlockMatches<MeshDraw>(entryPoint.PushConstants, meshDrawMembers);
	}
}

// The Renderer pushes GPU::GuiDrawPushConstants into the GUI pipeline, so the compiled shader must agree with it byte for byte.
CDL_TEST_CASE(EngineShaders, GuiPushBlockMatchesGuiDrawPushConstants, Unit)
{
	SkipUnlessCompiled(kGuiShaderPath);

	const SharedPtr<Shader> shader = LoadTestShader("Gui", kGuiShaderPath);

	using PushConstants = GPU::GuiDrawPushConstants;
	const std::array members = {
		CDL_INTEROP_MEMBER(PushConstants, Global),
		CDL_INTEROP_MEMBER(PushConstants, Vertices),
		CDL_INTEROP_MEMBER(PushConstants, Scale),
		CDL_INTEROP_MEMBER(PushConstants, Translate),
		CDL_INTEROP_MEMBER(PushConstants, TextureIndex),
		CDL_INTEROP_MEMBER(PushConstants, SamplerIndex) };
	for (const ShaderEntryPoint& entryPoint : shader->GetEntryPoints())
	{
		CDL_NOTE(entryPoint.Name);
		ExpectPushBlockMatches<PushConstants>(entryPoint.PushConstants, members);
	}
}

// The Renderer pushes GPU::DebugDrawPushConstants into the debug draw pipeline, so the compiled shader must agree with it byte for byte
CDL_TEST_CASE(EngineShaders, DebugDrawPushBlockMatchesDebugDrawPushConstants, Unit)
{
	SkipUnlessCompiled(kDebugDrawShaderPath);

	const SharedPtr<Shader> shader = LoadTestShader("DebugDraw", kDebugDrawShaderPath);

	using PushConstants = GPU::DebugDrawPushConstants;
	const std::array members = {
		CDL_INTEROP_MEMBER(PushConstants, Global),
		CDL_INTEROP_MEMBER(PushConstants, Vertices) };
	for (const ShaderEntryPoint& entryPoint : shader->GetEntryPoints())
	{
		// FragMain never touches the block, so the compiler is free to drop it there
		if (entryPoint.PushConstants.Members.empty())
			continue;

		CDL_NOTE(entryPoint.Name);
		ExpectPushBlockMatches<PushConstants>(entryPoint.PushConstants, members);
	}
}
