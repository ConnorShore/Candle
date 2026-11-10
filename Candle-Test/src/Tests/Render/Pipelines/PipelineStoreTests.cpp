// PipelineStore on a real device, from fixtures Build.py shaders compiles out of Candle-Test/res/shaders.
// Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"
#include "../ShaderInteropFixture.h"

#include <Candle/Render/Vulkan/BindlessTable.h>
#include <Candle/Render/Pipelines/PipelineSpecification.h>
#include <Candle/Render/Pipelines/PipelineStore.h>
#include <Candle/Render/Vulkan/RenderDevice.h>

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	// Relative to the repo root, which is where Build.py test and the Visual Studio debugger both run from.
	const std::filesystem::path kFixturePath = "Candle-Test/res/shaders/bin/PipelineFixture.spv";

	// A vertex and fragment pair whose push block follows the engine contract (see ShaderInteropFixture.h).
	const std::filesystem::path kInteropFixturePath = "Candle-Test/res/shaders/bin/ShaderInteropFixture.spv";

	// A vertex and fragment pair alongside two compute entry points, which no graphics pipeline can take as stages.
	const std::filesystem::path kShaderLoaderFixturePath = "Candle-Test/res/shaders/bin/ShaderLoaderFixture.spv";

	// The spec requires every implementation to support it as a blendable color attachment.
	constexpr vk::Format kColorFormat = vk::Format::eR8G8B8A8Unorm;

	// And as a depth attachment.
	constexpr vk::Format kDepthFormat = vk::Format::eD16Unorm;

	// The fixtures name their entry points after themselves, so their specs can't take the VertMain and FragMain defaults
	GraphicsPipelineSpecification PipelineFixtureSpec(SharedPtr<Shader> shader)
	{
		return {
			.Shader = std::move(shader),
			.VertexEntryPoint = "pipelineVertex",
			.FragmentEntryPoint = "pipelineFragment",
			.ColorAttachmentFormats = { kColorFormat } };
	}

	GraphicsPipelineSpecification InteropFixtureSpec(SharedPtr<Shader> shader)
	{
		return {
			.Shader = std::move(shader),
			.VertexEntryPoint = "interopVertex",
			.FragmentEntryPoint = "interopFragment",
			.ColorAttachmentFormats = { kColorFormat } };
	}

	ComputePipelineSpecification PipelineFixtureComputeSpec(SharedPtr<Shader> shader)
	{
		return { .Shader = std::move(shader), .EntryPoint = "pipelineCompute" };
	}

	// pipelineCompute's numthreads
	constexpr LocalGroupSize kFixtureComputeGroupSize{ 8, 4, 2 };

}

// 128 bytes is the spec's guaranteed minimum, so every device must accept the engine-wide range.
CDL_TEST_CASE(PipelineStore, SharedPushConstantRangeFitsTheDevice, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		CDL_EXPECT_LE(kPushConstantsSize, gpu.Device.GetPhysicalDevice().getProperties().limits.maxPushConstantsSize);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The Renderer binds set 0 and pushes at the top of a command buffer, before any pipeline is bound, so the global layout has
// to take both on its own. The push covers the whole range on every stage, which validation checks against the layout's one range.
CDL_TEST_CASE(PipelineStore, GlobalLayoutTakesTheSetAndTheWholeRangeWithNoPipelineBound, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		const PipelineStore store(gpu.Device, bindless);
		CDL_CHECK(store.GetGlobalLayout());
		TestCommands commands(gpu.Device);

		const std::array<std::byte, kPushConstantsSize> wholeRange{};
		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		bindless.BindSet(*commands.Buffer, vk::PipelineBindPoint::eGraphics, store.GetGlobalLayout());
		commands.Buffer.pushConstants(store.GetGlobalLayout(), kPushConstantStages, 0, kPushConstantsSize, wholeRange.data());
		commands.Buffer.end();
		SubmitAndWait(gpu.Device, commands.Buffer);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Validation checks the layout's range covers the shader's block in each stage using it. The fixture's C layout offsets are
// invalid std430, so this also fails if the device stops enabling scalarBlockLayout.
CDL_TEST_CASE(PipelineStore, CreatesFromAShaderFollowingThePushConstantContract, Gpu)
{
	SkipUnlessCompiled(kInteropFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);
		const GPUPipelineHandle handle = store.CreatePipeline(InteropFixtureSpec(LoadTestShader("ShaderInteropFixture", kInteropFixturePath)));

		CDL_EXPECT(handle.IsValid());
		CDL_EXPECT(store.TryGetPipeline(handle));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The fixture's compute entry points can't be stages of a graphics pipeline, so this only builds if the store makes stages of
// the two it was named and leaves the rest of the module alone.
CDL_TEST_CASE(PipelineStore, OnlyTheNamedEntryPointsBecomeStages, Gpu)
{
	SkipUnlessCompiled(kShaderLoaderFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);
		const SharedPtr<Shader> shader = LoadTestShader("ShaderLoaderFixture", kShaderLoaderFixturePath);
		CDL_CHECK_MSG(shader->GetEntryPoints().size() > 2, "the fixture holds no entry points beyond the two named, so this can't tell");

		const GPUPipelineHandle handle = store.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = shader,
			.VertexEntryPoint = "fixtureVertex",
			.FragmentEntryPoint = "fixtureFragment",
			.ColorAttachmentFormats = { kColorFormat } });
		CDL_EXPECT(store.TryGetPipeline(handle));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A vertex stage alone is a whole graphics pipeline, as a depth-only pass needs: no fragment stage and no colour attachment.
CDL_TEST_CASE(PipelineStore, BuildsAVertexOnlyPipelineForDepthOnlyPasses, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);

		const GPUPipelineHandle handle = store.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = LoadTestShader("PipelineFixture", kFixturePath),
			.VertexEntryPoint = "pipelineVertex",
			.FragmentEntryPoint = "",
			.DepthAttachmentFormat = kDepthFormat });
		CDL_EXPECT(store.TryGetPipeline(handle));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Every blend mode, with and without culling.
CDL_TEST_CASE(PipelineStore, CreatesForEachBlendModeWithoutValidationMessages, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);
		SharedPtr<Shader> shader = LoadTestShader("PipelineFixture", kFixturePath);

		for (BlendMode blend : { BlendMode::Opaque, BlendMode::AlphaBlend, BlendMode::PremultipliedAlpha, BlendMode::Additive })
		{
			for (bool cullBackFaces : { true, false })
			{
				GraphicsPipelineSpecification spec = PipelineFixtureSpec(shader);
				spec.Blend = blend;
				spec.CullBackFaces = cullBackFaces;
				const GPUPipelineHandle handle = store.CreatePipeline(spec);

				CDL_EXPECT_MSG(store.TryGetPipeline(handle), std::format("blend {}, cull {}", std::to_underlying(blend), cullBackFaces));
			}
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The Renderer keeps handles in place of pipelines, so each must keep leading to the pipeline it was created with as more are
// added after it and the store's storage grows. The two fixtures alternate, so neighbouring pipelines differ.
CDL_TEST_CASE(PipelineStore, EachHandleKeepsResolvingToItsOwnPipeline, Gpu)
{
	SkipUnlessCompiled(kFixturePath);
	SkipUnlessCompiled(kInteropFixturePath);
	constexpr size_t kPipelineCount = 9;

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);
		const SharedPtr<Shader> plain = LoadTestShader("PipelineFixture", kFixturePath);
		const SharedPtr<Shader> interop = LoadTestShader("ShaderInteropFixture", kInteropFixturePath);

		std::vector<std::pair<GPUPipelineHandle, vk::Pipeline>> created;
		for (size_t i = 0; i < kPipelineCount; ++i)
		{
			const GPUPipelineHandle handle = store.CreatePipeline(i % 2 == 0 ? PipelineFixtureSpec(plain) : InteropFixtureSpec(interop));
			created.emplace_back(handle, store.TryGetPipeline(handle));
			CDL_CHECK_MSG(created.back().second, std::format("pipeline {} didn't resolve as it was created", i));
		}

		for (size_t i = 0; i < created.size(); ++i)
		{
			const auto& [handle, pipeline] = created[i];
			CDL_EXPECT_MSG(store.TryGetPipeline(handle) == pipeline,
				std::format("handle {} resolves to another pipeline once {} more were created", i, created.size() - 1 - i));
			for (size_t j = 0; j < i; ++j)
				CDL_EXPECT_MSG(handle != created[j].first, std::format("pipelines {} and {} were given the same handle", j, i));
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A handle the store never gave out, the default one included, resolves to nothing rather than to some other pipeline.
CDL_TEST_CASE(PipelineStore, HandlesItNeverGaveOutResolveToNull, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);

		CDL_EXPECT_FALSE(store.TryGetPipeline(GPUPipelineHandle{ .Index = 0 }));

		const GPUPipelineHandle only = store.CreatePipeline(PipelineFixtureSpec(LoadTestShader("PipelineFixture", kFixturePath)));
		CDL_CHECK(store.TryGetPipeline(only));
		CDL_EXPECT_FALSE(store.TryGetPipeline(GPUPipelineHandle{}));
		CDL_EXPECT_FALSE(store.TryGetPipeline(GPUPipelineHandle{ .Index = only.Index + 1 }));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// From a file that also holds a vertex and fragment pair, as the Renderer's compute pipeline is built from Triangle.spv
CDL_TEST_CASE(PipelineStore, CreatesAComputePipelineFromTheNamedEntryPoint, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);

		const GPUPipelineHandle handle = store.CreatePipeline(PipelineFixtureComputeSpec(LoadTestShader("PipelineFixture", kFixturePath)));
		CDL_EXPECT(handle.IsValid());
		CDL_EXPECT(store.TryGetPipeline(handle));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The Renderer divides its dispatch by the group size it reads back here, so a compute pipeline must hand back the entry point
// it was built from. A default one's 1x1x1 still covers the target, just with far more groups than it needs, so only this shows it.
CDL_TEST_CASE(PipelineStore, GetEntryPointReturnsTheComputeEntryPointWithItsGroupSize, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);
		const GPUPipelineHandle handle = store.CreatePipeline(PipelineFixtureComputeSpec(LoadTestShader("PipelineFixture", kFixturePath)));
		CDL_CHECK(store.TryGetPipeline(handle));

		const std::expected<ShaderEntryPoint, std::string> entryPoint = store.GetEntryPoint(handle, ShaderStage::Compute);
		CDL_CHECK_MSG(entryPoint.has_value(), entryPoint.error());
		CDL_EXPECT_EQ(entryPoint->Name, std::string("pipelineCompute"));
		CDL_EXPECT_EQ(entryPoint->Stage, ShaderStage::Compute);
		ExpectGroupSize(entryPoint->LocalGroupSize, kFixtureComputeGroupSize, "pipelineCompute");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The two stages sit side by side in the store, so asking for one must not hand back the other
CDL_TEST_CASE(PipelineStore, GetEntryPointReturnsEachStageOfAGraphicsPipeline, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);
		const GPUPipelineHandle handle = store.CreatePipeline(PipelineFixtureSpec(LoadTestShader("PipelineFixture", kFixturePath)));
		CDL_CHECK(store.TryGetPipeline(handle));

		constexpr std::pair<ShaderStage, std::string_view> kStages[] = {
			{ ShaderStage::Vertex, "pipelineVertex" },
			{ ShaderStage::Fragment, "pipelineFragment" } };
		for (const auto& [stage, name] : kStages)
		{
			const std::expected<ShaderEntryPoint, std::string> entryPoint = store.GetEntryPoint(handle, stage);
			CDL_CHECK_MSG(entryPoint.has_value(), entryPoint.error());
			CDL_EXPECT_EQ(entryPoint->Name, std::string(name));
			CDL_EXPECT_EQ(entryPoint->Stage, stage);
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// As with TryGetPipeline, a handle the store never gave out leads to nothing, here an error rather than some other entry point
CDL_TEST_CASE(PipelineStore, GetEntryPointFailsForHandlesItNeverGaveOut, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);

		CDL_EXPECT_FALSE(store.GetEntryPoint(GPUPipelineHandle{ .Index = 0 }, ShaderStage::Compute).has_value());

		const GPUPipelineHandle only = store.CreatePipeline(PipelineFixtureComputeSpec(LoadTestShader("PipelineFixture", kFixturePath)));
		CDL_CHECK(store.TryGetPipeline(only));
		CDL_EXPECT_FALSE(store.GetEntryPoint(GPUPipelineHandle{}, ShaderStage::Compute).has_value());
		CDL_EXPECT_FALSE(store.GetEntryPoint(GPUPipelineHandle{ .Index = only.Index + 1 }, ShaderStage::Compute).has_value());
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Every pipeline is built on the global layout, so the Renderer binds set 0 once per command buffer rather than once per
// pipeline. Probes alternate between two pipelines with the set bound once before either, so a pipeline whose layout disagreed
// at set 0 fails validation at its draws, and one that lost the binding samples the wrong colour.
CDL_TEST_CASE(PipelineStore, PipelinesShareOneBindingOfTheGlobalSet, Gpu)
{
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const std::array colours = { kRed, kGreen, kBlue, kWhite };
		std::vector<TextureProbe> probes;
		for (const Texel& colour : colours)
			probes.push_back({ .Texture = CreateBoundTexture(gpu, { 1, 1 }, std::array<Texel, 1>{ colour }), .Sampler = SamplerType::PointClamp, .UV = { 0.5f, 0.5f } });

		const std::vector<Texel> sampled = SampleTextures(gpu, probes, 2);
		CDL_CHECK_EQ(sampled.size(), colours.size());
		for (size_t i = 0; i < colours.size(); ++i)
			ExpectTexelNear(sampled[i], colours[i], std::format("probe {}, drawn with pipeline {}", i, i % 2), 0);
	}
	ExpectNoVulkanDebugMessages(logs);
}

namespace {

	// A PipelineFixture spec with these entry points in its slots, which the store must turn away
	struct RejectedCase
	{
		const char* Label;
		std::string Vertex;
		std::string Fragment;
		std::string AtFault;	// The entry point the logged reason must name; empty when no name is at fault
	};

	// The same for a compute spec over PipelineFixture
	struct RejectedComputeCase
	{
		const char* Label;
		std::string EntryPoint;
		std::string AtFault;
	};

	PipelineSpecification RejectedSpec(SharedPtr<Shader> shader, const RejectedCase& rejected)
	{
		GraphicsPipelineSpecification spec = PipelineFixtureSpec(std::move(shader));
		spec.VertexEntryPoint = rejected.Vertex;
		spec.FragmentEntryPoint = rejected.Fragment;
		return spec;
	}

	PipelineSpecification RejectedSpec(SharedPtr<Shader> shader, const RejectedComputeCase& rejected)
	{
		return ComputePipelineSpecification{ .Shader = std::move(shader), .EntryPoint = rejected.EntryPoint };
	}

	// The validation layer logs on Render too, so its messages are left to ExpectNoVulkanDebugMessages
	std::vector<std::string> PipelineErrors(LoggerFixture& logs)
	{
		CDL_CHECK(Logger::Flush());
		std::vector<std::string> errors;
		for (size_t i = 0; i < logs.Records.size(); ++i)
		{
			const LogRecord& record = logs.Records[i];
			if (record.Level == LogLevel::Error && record.Channel == LogChannel::Render && !record.IsContinuation
				&& !std::string_view(record.Message).starts_with("[Vulkan Debug]"))
				errors.push_back(JoinedMessage(logs.Records, i));
		}
		return errors;
	}

	// Each case must come back as an invalid handle before Vulkan sees the spec, and since the handle can't say why, log one
	// error naming the entry point at fault. What was built before must still resolve as it did.
	template<typename Case, size_t N>
	void ExpectEachRejected(LoggerFixture& logs, const Case (&cases)[N])
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);
		const SharedPtr<Shader> shader = LoadTestShader("PipelineFixture", kFixturePath);
		const GPUPipelineHandle built = store.CreatePipeline(PipelineFixtureSpec(shader));
		const vk::Pipeline builtPipeline = store.TryGetPipeline(built);
		CDL_CHECK(builtPipeline);

		for (const Case& rejected : cases)
		{
			const size_t errorsBefore = PipelineErrors(logs).size();
			const GPUPipelineHandle handle = store.CreatePipeline(RejectedSpec(shader, rejected));
			const std::vector<std::string> errors = PipelineErrors(logs);

			CDL_EXPECT_MSG(!handle.IsValid(), std::format("{} gave a valid handle", rejected.Label));
			CDL_EXPECT_MSG(!store.TryGetPipeline(handle), std::format("{} resolved to a pipeline", rejected.Label));
			if (errors.size() != errorsBefore + 1)
			{
				CDL_EXPECT_MSG(false, std::format("{} logged {} errors, expected one", rejected.Label, errors.size() - errorsBefore));
				continue;
			}
			CDL_EXPECT_MSG(errors.back().contains(rejected.AtFault),
				std::format("{}: '{}' doesn't name {}", rejected.Label, errors.back(), rejected.AtFault));
		}

		CDL_EXPECT(store.TryGetPipeline(built) == builtPipeline);
	}

}

// The defaults are the engine shaders' names, so leaving them on a shader that names its own is the likely slip. A fragment
// stage is optional but a vertex stage isn't, so naming only a fragment entry point is turned away too.
CDL_TEST_CASE(PipelineStore, RejectsEntryPointsTheShaderDoesNotHave, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	const GraphicsPipelineSpecification defaults{};
	const RejectedCase cases[] = {
		{ "the defaults", defaults.VertexEntryPoint, defaults.FragmentEntryPoint, defaults.VertexEntryPoint },
		{ "a misnamed vertex entry point", "missingVertex", "pipelineFragment", "missingVertex" },
		{ "a misnamed fragment entry point", "pipelineVertex", "missingFragment", "missingFragment" },
		{ "no vertex entry point", "", "pipelineFragment", "" },
		{ "no entry points", "", "", "" } };

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	ExpectEachRejected(logs, cases);
	ExpectNoVulkanDebugMessages(logs);
}

// Each entry point's stage comes from reflection, not its slot, so without the check the swapped pair would quietly build and the
// vertex entry point in both slots would build two vertex stages. Swapping trips the vertex slot's check, the other the fragment's.
CDL_TEST_CASE(PipelineStore, RejectsEntryPointsNamedForTheWrongStage, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	const RejectedCase cases[] = {
		{ "the pair swapped", "pipelineFragment", "pipelineVertex", "pipelineFragment" },
		{ "the vertex entry point in both slots", "pipelineVertex", "pipelineVertex", "pipelineVertex" } };

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	ExpectEachRejected(logs, cases);
	ExpectNoVulkanDebugMessages(logs);
}

// The default is the engine shaders' name, as for graphics specs. A stage comes from reflection, not the spec, so without the
// check a vertex or fragment entry point would reach Vulkan as a compute pipeline's one stage.
CDL_TEST_CASE(PipelineStore, RejectsComputeEntryPointsTheShaderDoesNotHaveOrThatAreNotCompute, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	const ComputePipelineSpecification defaults{};
	const RejectedComputeCase cases[] = {
		{ "the default", defaults.EntryPoint, defaults.EntryPoint },
		{ "a misnamed compute entry point", "missingCompute", "missingCompute" },
		{ "no entry point", "", "" },
		{ "a vertex entry point", "pipelineVertex", "pipelineVertex" },
		{ "a fragment entry point", "pipelineFragment", "pipelineFragment" } };

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	ExpectEachRejected(logs, cases);
	ExpectNoVulkanDebugMessages(logs);
}

// A shader that failed to load leaves the spec's pointer null, which must come back as an invalid handle rather than a crash
CDL_TEST_CASE(PipelineStore, RejectsASpecWithNoShader, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		BindlessTable bindless(gpu.Device);
		PipelineStore store(gpu.Device, bindless);

		CDL_EXPECT_FALSE(store.CreatePipeline(GraphicsPipelineSpecification{}).IsValid());
		CDL_EXPECT_FALSE(store.CreatePipeline(ComputePipelineSpecification{}).IsValid());
		CDL_EXPECT_EQ(PipelineErrors(logs).size(), 2u);
	}
	ExpectNoVulkanDebugMessages(logs);
}
