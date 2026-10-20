// The C++/Slang interop contract: interop types are plain bytes on the C++ side, and a shader compiled from the same
// header lays them out exactly as C++ does. The GPU test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"
#include "ShaderInteropFixture.h"

#include <Candle/Render/Pipelines/TriangleInterop.h>
#include <Candle/Render/Shader.h>
#include <Candle/Render/ShaderInterop/FrameData.h>
#include <Candle/Render/ShaderInterop/GlobalPushConstants.h>
#include <Candle/Render/ShaderInterop/Vertex.h>
#include <Candle/Render/ShaderLoader.h>

#include <array>
#include <filesystem>
#include <type_traits>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	// Interop types are memcpy'd into GPU memory and pushed as raw bytes, and offsetof needs standard layout.
	template<typename T>
	constexpr bool kIsPlainBytes = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

	const std::filesystem::path kFixturePath = "Candle-Test/res/shaders/bin/ShaderInteropFixture.spv";

}

CDL_STATIC_ASSERT(kIsPlainBytes<GPU::Vertex>);
CDL_STATIC_ASSERT(kIsPlainBytes<GPU::FrameData>);
CDL_STATIC_ASSERT(kIsPlainBytes<GPU::GlobalPushConstants>);
CDL_STATIC_ASSERT(kIsPlainBytes<GPU::MeshDrawPushConstants>);
CDL_STATIC_ASSERT(kIsPlainBytes<GPU::InteropFixturePushConstants>);

// A shader pointer is a vk::DeviceAddress on the C++ side.
CDL_STATIC_ASSERT(sizeof(CDL_GPU_PTR(GPU::Vertex)) == sizeof(vk::DeviceAddress));

// Every member sits where std430 would put it elsewhere, so this fails if CompileShaders.py stops passing
// -fvk-use-c-layout, or if either language's definition drifts from the shared header.
CDL_TEST_CASE(ShaderInterop, ShaderLaysOutTheFixtureExactlyAsCppDoes, Gpu)
{
	SkipUnlessCompiled(kFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestDevice gpu;
		SharedPtr<Shader> shader = ShaderLoader::LoadShader("ShaderInteropFixture", kFixturePath, gpu.Device);

		using Fixture = GPU::InteropFixturePushConstants;
		const std::array members = {
			CDL_INTEROP_MEMBER(Fixture, Global),
			CDL_INTEROP_MEMBER(Fixture, Scalar),
			CDL_INTEROP_MEMBER(Fixture, Vector3),
			CDL_INTEROP_MEMBER(Fixture, Vector4),
			CDL_INTEROP_MEMBER(Fixture, Count),
			CDL_INTEROP_MEMBER(Fixture, Vector2),
			CDL_INTEROP_MEMBER(Fixture, EmbeddedVertex),
			CDL_INTEROP_MEMBER(Fixture, EmbeddedFrame),
			CDL_INTEROP_MEMBER(Fixture, Vertices) };

		CDL_CHECK_EQ(shader->GetEntryPoints().size(), 2u);
		for (const ShaderEntryPoint& entryPoint : shader->GetEntryPoints())
		{
			CDL_NOTE(entryPoint.Name);
			ExpectPushBlockMatches(entryPoint.m_PushConstantBlock, sizeof(Fixture), members);
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}
