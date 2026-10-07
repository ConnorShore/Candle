// ShaderLoader on a real device: reflection of a fixture that Build.py shaders compiles from
// Candle-Test/res/shaders, and rejection of files that are not SPIR-V. Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/RenderDevice.h>
#include <Candle/Render/RenderInstance.h>
#include <Candle/Render/Shader.h>
#include <Candle/Render/ShaderLoader.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

using namespace Candle;
using namespace Candle::Test;
using namespace std::string_view_literals;
using Candle::Test::Type::Gpu;

namespace {

	// Relative to the repo root, which is where Build.py test and the Visual Studio debugger both run from.
	const std::filesystem::path kFixturePath = "Candle-Test/res/shaders/bin/ShaderLoaderFixture.spv";

	// The fixture is a build output, not a checked-in file, so a missing one is a setup step rather than a failure.
	std::vector<std::byte> ReadFixture()
	{
		auto fixture = Platform::ReadFile(kFixturePath);
		if (!fixture && fixture.error().Code == FileErrorCode::NotFound)
			CDL_SKIP(std::format("{} is not compiled; run Build.py shaders", kFixturePath.string()));
		CDL_CHECK_MSG(fixture.has_value(), std::format("reading the fixture failed: {}", ToString(fixture.error().Code)));
		return std::move(*fixture);
	}

	// Members construct in order and destroy in reverse, so the device goes before the instance and the backend.
	struct TestDevice
	{
		WindowingBackend Backend;
		RenderSpecification Spec = StrictRenderSpec();
		RenderInstance Instance{ TestInstanceSpec(Spec) };
		RenderDevice Device{ Instance, Spec };
	};

	// The loader still reports failure by throwing; returns the message, or nullopt if the load succeeded.
	std::optional<std::string> LoadError(const std::filesystem::path& path, RenderDevice& device)
	{
		try { ShaderLoader::LoadShader("Rejected", path, device); }
		catch (const std::exception& e) { return e.what(); }
		return std::nullopt;
	}

	void ExpectRejectedNaming(const std::filesystem::path& path, RenderDevice& device)
	{
		const std::optional<std::string> error = LoadError(path, device);
		CDL_EXPECT_MSG(error.has_value(), std::format("{} loaded", path.filename().string()));
		if (error)
		{
			CDL_NOTE(*error);
			CDL_EXPECT_MSG(error->contains(path.filename().string()), "the message should name the file");
		}
	}

	const ShaderEntryPoint* FindEntryPoint(const Shader& shader, std::string_view name)
	{
		const auto& entryPoints = shader.GetEntryPoints();
		const auto it = std::ranges::find(entryPoints, name, &ShaderEntryPoint::Name);
		return it == entryPoints.end() ? nullptr : &*it;
	}

}

// The names also check that CompileShaders.py's -fvk-use-entrypoint-name survived; without it every one is "main".
CDL_TEST_CASE(ShaderLoader, ReflectsEveryEntryPointWithItsStage, Gpu)
{
	ReadFixture();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		const Shader shader = ShaderLoader::LoadShader("Fixture", kFixturePath, gpu.Device);

		CDL_EXPECT_EQ(shader.GetName(), std::string("Fixture"));
		CDL_EXPECT(shader.GetFilePath() == kFixturePath);
		CDL_EXPECT(*shader.GetModule());
		CDL_EXPECT_EQ(shader.GetEntryPoints().size(), 4u);

		constexpr std::pair<std::string_view, ShaderStage> kExpected[] = {
			{ "fixtureVertex"sv, ShaderStage::Vertex },
			{ "fixtureFragment"sv, ShaderStage::Fragment },
			{ "fixtureClear"sv, ShaderStage::Compute },
			{ "fixtureIncrement"sv, ShaderStage::Compute },
		};
		for (const auto& [name, stage] : kExpected)
		{
			const ShaderEntryPoint* entryPoint = FindEntryPoint(shader, name);
			CDL_EXPECT_MSG(entryPoint != nullptr, std::format("no entry point named {}", name));
			if (entryPoint)
				CDL_EXPECT_EQ(entryPoint->Stage, stage);
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Each is caught before the bytes reach the driver: by the open, the size check or the magic number.
CDL_TEST_CASE(ShaderLoader, RejectsFilesThatAreNotSpirv, Gpu)
{
	TempDirectory dir;
	const std::byte notMultipleOfFour[6] = {};
	CDL_CHECK(Platform::WriteFile(dir / "empty.spv", {}).has_value());
	CDL_CHECK(Platform::WriteFile(dir / "six-bytes.spv", notMultipleOfFour).has_value());
	// Twelve bytes, so it passes the size check and only the magic number can reject it.
	CDL_CHECK(Platform::WriteFile(dir / "bad-magic.spv", std::as_bytes(std::span("not SPIR-V!!"sv))).has_value());

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		ExpectRejectedNaming(dir / "missing.spv", gpu.Device);
		ExpectRejectedNaming(dir / "empty.spv", gpu.Device);
		ExpectRejectedNaming(dir / "six-bytes.spv", gpu.Device);
		ExpectRejectedNaming(dir / "bad-magic.spv", gpu.Device);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A real header over a cut-off body, as a crashed compile or a half-written hot reload leaves behind. It must be
// rejected without the driver seeing it: vkCreateShaderModule requires valid SPIR-V, and anything else is undefined.
CDL_TEST_CASE(ShaderLoader, RejectsTruncatedSpirvBeforeTheDriverSeesIt, Gpu)
{
	// Before reflection ran first, this input crashed the validation layer (an access violation in
	// VkLayer_khronos_validation.dll, SDK 1.4.357) and took the whole run with it.
	std::vector<std::byte> truncated = ReadFixture();
	truncated.resize((truncated.size() / 2) & ~size_t{ 3 });

	TempDirectory dir;
	CDL_CHECK(Platform::WriteFile(dir / "truncated.spv", truncated).has_value());

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		ExpectRejectedNaming(dir / "truncated.spv", gpu.Device);
	}
	ExpectNoVulkanDebugMessages(logs);
}
