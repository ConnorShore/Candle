// BindlessTable on a real device: set 0's layout, its texture writes, its binding against a pipeline layout, and what a
// shader reads through it. Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/BindlessTable.h>
#include <Candle/Render/Pipeline.h>
#include <Candle/Render/PipelineSpecification.h>
#include <Candle/Render/RenderConstants.h>
#include <Candle/Render/SamplerType.h>
#include <Candle/Render/ShaderLoader.h>
#include <Candle/Render/TexturePool.h>

#include <array>
#include <filesystem>
#include <format>
#include <iterator>
#include <type_traits>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

// A copy would be a second owner freeing the same descriptor set, layout and samplers.
CDL_STATIC_ASSERT(!std::is_copy_constructible_v<BindlessTable>);
CDL_STATIC_ASSERT(!std::is_copy_assignable_v<BindlessTable>);

namespace {

	// Samples nothing, so it shows the bindless set is part of every layout, not just those whose shaders use it.
	const std::filesystem::path kPipelineFixturePath = "Candle-Test/res/shaders/bin/PipelineFixture.spv";

	constexpr vk::Format kColorFormat = vk::Format::eR8G8B8A8Unorm;

	Pipeline CreatePipeline(TestTextures& gpu, const std::filesystem::path& shaderPath)
	{
		return Pipeline(PipelineSpecification{
			.Shader = ShaderLoader::LoadShader(shaderPath.stem().string(), shaderPath, gpu.Device),
			.ColorAttachmentFormats = { kColorFormat } }, gpu.Device, gpu.Bindless, kColorFormat);
	}

	// Each SamplerType's address mode and filter, told apart on a red | blue texture. Half a texel left of the edge is
	// the centre of the texel wrapping reaches, blue, while clamping stays on red; a quarter texel past red's centre,
	// filtering blends a quarter of blue in. Both lie on a 1/4 texel step, which the spec's minimum 4 bits of subtexel
	// precision represent exactly.
	constexpr float kLeftOfTheEdge = -0.25f;
	constexpr float kQuarterTexelPastRed = 0.375f;
	constexpr Texel kRedBlend{ 191, 0, 64, 255 };

	struct SamplerCase
	{
		SamplerType Sampler;
		const char* Name;
		Texel LeftOfTheEdge;
		Texel QuarterTexelPastRed;
	};

	constexpr SamplerCase kSamplerCases[] = {
		{ SamplerType::LinearWrap, "LinearWrap", kBlue, kRedBlend },
		{ SamplerType::LinearClamp, "LinearClamp", kRed, kRedBlend },
		{ SamplerType::PointWrap, "PointWrap", kBlue, kRed },
		{ SamplerType::PointClamp, "PointClamp", kRed, kRed } };
	CDL_STATIC_ASSERT(std::size(kSamplerCases) == static_cast<size_t>(SamplerType::Count), "A new SamplerType needs a case here");

}

CDL_TEST_CASE(BindlessTable, CreatesWithoutValidationMessages, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const BindlessTable bindless(gpu.Device);

		CDL_EXPECT(bindless.GetSet());
		CDL_EXPECT(bindless.GetLayout());
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Validation checks each write against the binding's descriptor count, so the last element proves it is kMaxTextures long.
CDL_TEST_CASE(BindlessTable, WritesTheFirstAndLastTextureElements, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle texture = CreateBoundTexture(gpu, { 1, 1 }, std::array<Texel, 1>{ kWhite });
		const vk::ImageView view = gpu.Pool.GetTextureInfo(texture).ImageView;

		gpu.Bindless.WriteTexture(0, view);
		gpu.Bindless.WriteTexture(kMaxTextures - 1, view);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Validation checks the bound set's layout against set 0 of the pipeline layout, which must be the table's own.
CDL_TEST_CASE(BindlessTable, BindsAgainstThePipelineLayoutsItIsBuiltInto, Gpu)
{
	SkipUnlessCompiled(kPipelineFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		Pipeline pipeline = CreatePipeline(gpu, kPipelineFixturePath);
		vk::PipelineLayout layout = *pipeline.GetLayout();
		TestCommands commands(gpu.Device);

		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		gpu.Bindless.BindSet(*commands.Buffer, vk::PipelineBindPoint::eGraphics, layout);
		commands.Buffer.end();
		SubmitAndWait(gpu.Device, commands.Buffer);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The main thread publishes textures while the render thread has the set bound. Without UPDATE_AFTER_BIND, a write to
// a set bound in a recorded command buffer invalidates that buffer, so submitting it would be an error.
CDL_TEST_CASE(BindlessTable, WritingWhileBoundLeavesTheCommandBufferSubmittable, Gpu)
{
	SkipUnlessCompiled(kPipelineFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		Pipeline pipeline = CreatePipeline(gpu, kPipelineFixturePath);
		vk::PipelineLayout layout = *pipeline.GetLayout();
		TestCommands commands(gpu.Device);

		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		gpu.Bindless.BindSet(*commands.Buffer, vk::PipelineBindPoint::eGraphics, layout);
		CreateBoundTexture(gpu, { 1, 1 }, std::array<Texel, 1>{ kWhite });
		commands.Buffer.end();
		SubmitAndWait(gpu.Device, commands.Buffer);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Shaders index binding 1 with a SamplerType, so element i must be the sampler GetSamplerInfo describes for type i.
CDL_TEST_CASE(BindlessTable, SamplersAreIndexedBySamplerType, Gpu)
{
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const GPUTextureHandle texture = CreateBoundTexture(gpu, { 2, 1 }, std::array{ kRed, kBlue });

		std::vector<TextureProbe> probes;
		for (const SamplerCase& c : kSamplerCases)
		{
			probes.push_back({ .Texture = texture, .Sampler = c.Sampler, .UV = { kLeftOfTheEdge, 0.5f } });
			probes.push_back({ .Texture = texture, .Sampler = c.Sampler, .UV = { kQuarterTexelPastRed, 0.5f } });
		}

		const std::vector<Texel> sampled = SampleTextures(gpu, probes);
		CDL_CHECK_EQ(sampled.size(), probes.size());
		for (size_t i = 0; i < std::size(kSamplerCases); ++i)
		{
			const SamplerCase& c = kSamplerCases[i];
			ExpectTexelNear(sampled[2 * i], c.LeftOfTheEdge, std::format("{} left of the edge", c.Name));
			ExpectTexelNear(sampled[2 * i + 1], c.QuarterTexelPastRed, std::format("{} a quarter texel past red", c.Name));
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Shaders index binding 0 with the handle's slot, so each texture must be written at its own element and no other.
CDL_TEST_CASE(BindlessTable, EachTextureIsSampledFromItsOwnSlot, Gpu)
{
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestTextures gpu;
		const std::array colours = { kRed, kGreen, kBlue };
		std::vector<TextureProbe> probes;
		for (const Texel& colour : colours)
			probes.push_back({ .Texture = CreateBoundTexture(gpu, { 1, 1 }, std::array<Texel, 1>{ colour }), .Sampler = SamplerType::PointClamp, .UV = { 0.5f, 0.5f } });

		const std::vector<Texel> sampled = SampleTextures(gpu, probes);
		CDL_CHECK_EQ(sampled.size(), colours.size());
		for (size_t i = 0; i < colours.size(); ++i)
			ExpectTexelNear(sampled[i], colours[i], std::format("texture {} at slot {}", i, probes[i].Texture.Index), 0);
	}
	ExpectNoVulkanDebugMessages(logs);
}
