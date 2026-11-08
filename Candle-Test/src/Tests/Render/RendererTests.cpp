// Renderer on a real device, drawing packets into an image the test owns in place of a swapchain image and reading the
// pixels back, so a test sees what a frame really shows: the scene, the GUI over it, and, when rendering to an output
// target, the scene in that target for the GUI to show. Each test fails on any validation message, synchronization
// validation's included, which checks the barriers between the passes.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Debug/ConsoleManager.h>
#include <Candle/Debug/DebugDrawManager.h>
#include <Candle/Render/FrameContext.h>
#include <Candle/Render/FramePacket.h>
#include <Candle/Render/GUI/GuiDrawData.h>
#include <Candle/Render/Renderer.h>
#include <Candle/Render/ShaderInterop/FrameData.h>
#include <Candle/Render/Vulkan/SwapChainImage.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	constexpr vk::Format kTargetFormat = vk::Format::eR8G8B8A8Unorm;	// UNORM, so the shader's result is stored unconverted
	constexpr vk::Extent2D kExtent{ 4, 4 };
	const glm::vec2 kDisplaySize{ kExtent.width, kExtent.height };	// ImGui's display, one unit per target pixel
	constexpr uint32_t kMaxTestTransforms = 4;

	// Covers clip space at z = 0, wound counter-clockwise like kTriangleVertices so culling keeps it
	const std::array<GPU::Vertex, 4> kQuadVertices = { {
		{ { -1.0f, -1.0f, 0.0f } },
		{ { 1.0f, -1.0f, 0.0f } },
		{ { 1.0f, 1.0f, 0.0f } },
		{ { -1.0f, 1.0f, 0.0f } } } };
	constexpr std::array<uint32_t, 6> kQuadIndices = { 0, 1, 2, 0, 2, 3 };

	// Triangle.slang tints by the packet's time, which at 0 keeps only red, so these differ in red alone
	constexpr Texel kBright{ 255, 0, 0, 255 };
	constexpr Texel kDim{ 96, 0, 0, 255 };
	constexpr Texel kCleared{ 0, 0, 0, 255 };	// Renderer's clear colour, where nothing drew

	// Render leaves its target ready to present, with no stage or access, which is the state SwapChainImage then tracks
	constexpr ImageState kPresentState{ .Layout = vk::ImageLayout::ePresentSrcKHR };

	// One slot of what RenderManager::CreateFrameContexts builds, with room for the few transforms a test draws
	FrameContext CreateFrameContext(TestAllocator& gpu)
	{
		FrameContext frame;
		vk::CommandPoolCreateInfo poolInfo{ .queueFamilyIndex = gpu.Device.GetQueueFamily(QueueType::Graphics) };
		frame.CommandPool = vk::raii::CommandPool(gpu.Device.GetDevice(), poolInfo);

		vk::CommandBufferAllocateInfo allocInfo{ .commandPool = *frame.CommandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
		frame.CommandBuffer = std::move(vk::raii::CommandBuffers(gpu.Device.GetDevice(), allocInfo).front());

		frame.FrameData = gpu.Allocator.CreateBuffer({
			.Size = sizeof(GPU::FrameData),
			.Usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
			.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT });
		frame.TransformData = gpu.Allocator.CreateBuffer({
			.Size = kMaxTestTransforms * sizeof(glm::mat4),
			.Usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
			.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT });
		return frame;
	}

	// Where the scene pass draws: straight to the target, or into the packet's output target for the GUI to show.
	enum class RenderTo { SwapChain, OutputTarget };

	// What RenderManager holds around its Renderer, in the same order, plus one frame slot. Each frame is waited on but the
	// mesh and texture uploads are not, so the GPU is drained before the pools go.
	struct TestRenderer : TestTextures
	{
		GeometryPool Geometry{ Allocator, Uploader, Registry };
		Renderer SceneRenderer;
		FrameContext Frame = CreateFrameContext(*this);

		explicit TestRenderer(vk::Extent2D extent, RenderTo renderTo = RenderTo::SwapChain)
			: SceneRenderer(Device, Geometry, Pool, Bindless, Allocator, {
				.TargetFormat = kTargetFormat,
				.TargetExtent = extent,
				.ShaderDirectory = kEngineShaderDirectory,
				.RenderToSwapChain = renderTo == RenderTo::SwapChain }) {}
		~TestRenderer() { Device.WaitIdle(); }

		GPUMeshHandle CreateQuad() { return Geometry.CreateMesh(kQuadVertices, kQuadIndices); }
		GPUTextureHandle CreateSolidTexture(const Texel& colour) { return CreateBoundTexture(*this, { 1, 1 }, std::array<Texel, 1>{ colour }); }

		// As RenderManager::CreateOutputTarget makes one: in the format the scene's pipelines draw to
		GPUTextureHandle CreateOutputTarget(glm::uvec2 extent)
		{
			return Pool.CreateRenderTarget({ .Extent = extent, .Format = SceneRenderer.GetTargetFormat(), .DebugName = "Test Output Target" });
		}
	};

	// Stands in for a swapchain image: SwapChainImage over an image the test owns, which TransferSrc lets it read back.
	// The image is declared first, so the view goes before it.
	struct TestTarget
	{
		OwnedImage Owned;
		SwapChainImage Image;
	};

	TestTarget CreateTarget(RenderDevice& device, vk::Extent2D extent)
	{
		OwnedImage owned = CreateOwnedImage(device, kTargetFormat, extent, 1, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
		const vk::Image image = *owned.Handle;
		vk::raii::ImageView view(device.GetDevice(), vk::ImageViewCreateInfo{
			.image = image, .viewType = vk::ImageViewType::e2D, .format = kTargetFormat, .subresourceRange = SwapChainImage::kSubresourceRange });
		return { std::move(owned), SwapChainImage(image, std::move(view), ImageDesc{ .Extent = extent, .Format = kTargetFormat }) };
	}

	// One frame as RenderManager draws it, without the acquire and present: record, submit and wait, then read the target.
	std::vector<Texel> RenderFrame(TestRenderer& test, TestTarget& target, const FramePacket& packet)
	{
		test.Frame.CommandPool.reset();
		test.SceneRenderer.Render(packet, test.Frame, target.Image);
		SubmitAndWait(test.Device, test.Frame.CommandBuffer);

		std::vector<Texel> texels = ToTexels(ReadBackImage(test, *target.Owned.Handle, target.Image.GetDesc().Extent, kPresentState));
		target.Image.SetState(kTransferSrcState);	// Where the readback left it, so the next frame's first barrier starts from there
		return texels;
	}

	// One quad drawn with one texture. View and projection stay identity, so the transform alone places it in clip space.
	struct Layer
	{
		GPUTextureHandle Texture;
		glm::mat4 Transform;
	};

	glm::mat4 AtDepth(float depth) { return glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, depth)); }

	// The layers in draw order, each with its own transform; Time stays 0, as kBright and kDim assume
	FramePacket MakePacket(GPUMeshHandle quad, std::initializer_list<Layer> layers)
	{
		FramePacket packet;
		for (const Layer& layer : layers)
		{
			packet.DrawItems.push_back({
				.Mesh = quad,
				.Texture = layer.Texture,
				.Sampler = SamplerType::PointClamp,
				.TransformIndex = static_cast<uint32_t>(packet.TransformData.size()) });
			packet.TransformData.push_back(layer.Transform);
		}
		return packet;
	}

	// What a texture shades to when drawn alone, so tests compare against the shader's real output rather than a guess at it
	Texel ShadeAlone(TestRenderer& test, TestTarget& target, GPUMeshHandle quad, GPUTextureHandle texture)
	{
		const Texel colour = RenderFrame(test, target, MakePacket(quad, { { texture, AtDepth(0.5f) } })).front();
		CDL_CHECK_MSG(colour != kCleared, "a full-screen quad drew nothing, so nothing here can be compared");
		return colour;
	}

	void CheckDistinguishable(const Texel& a, const Texel& b)
	{
		CDL_CHECK_MSG(a != b, std::format("both textures shade to {}, so the test can't tell which one drew; has Triangle.slang's tint changed?", a));
	}

	// Names the first texel that differs, rather than one failure per texel
	void ExpectEveryTexel(std::span<const Texel> texels, const Texel& expected, std::string_view label)
	{
		for (size_t i = 0; i < texels.size(); ++i)
		{
			if (texels[i] != expected)
			{
				CDL_EXPECT_MSG(false, std::format("{}: texel {} is {}, expected {} everywhere", label, i, texels[i], expected));
				return;
			}
		}
	}

	// An expected frame: one colour everywhere, with Paint drawing rectangles of others over it.
	std::vector<Texel> Solid(const Texel& colour, vk::Extent2D extent = kExtent)
	{
		return std::vector<Texel>(size_t{ extent.width } * extent.height, colour);
	}

	// Fills the pixels in [min, max)
	void Paint(std::vector<Texel>& image, glm::uvec2 min, glm::uvec2 max, const Texel& colour, vk::Extent2D extent = kExtent)
	{
		for (uint32_t y = min.y; y < max.y; ++y)
			for (uint32_t x = min.x; x < max.x; ++x)
				image[y * extent.width + x] = colour;
	}

	bool IsNear(const Texel& a, const Texel& b, int tolerance)
	{
		for (size_t channel = 0; channel < a.size(); ++channel)
			if (std::abs(int{ a[channel] } - int{ b[channel] }) > tolerance)
				return false;
		return true;
	}

	// Names the first pixel that differs, by position, rather than one failure per texel
	void ExpectImage(std::span<const Texel> texels, std::span<const Texel> expected, std::string_view label, int tolerance = 0,
		vk::Extent2D extent = kExtent)
	{
		CDL_CHECK_EQ(texels.size(), expected.size());
		for (size_t i = 0; i < texels.size(); ++i)
		{
			if (!IsNear(texels[i], expected[i], tolerance))
			{
				CDL_EXPECT_MSG(false, std::format("{}: pixel ({}, {}) is {}, expected {}", label, i % extent.width, i / extent.width, texels[i], expected[i]));
				return;
			}
		}
	}

	// Every texel of a render target, read through its bindless slot as the GUI samples it, in row order
	std::vector<Texel> SampleRenderTarget(TestRenderer& test, GPUTextureHandle target)
	{
		return SampleTextures(test, ProbeEveryTexel(target, test.Pool.GetTextureInfo(target).Extent));
	}

	// ShadeAlone for a renderer drawing into an output target: the texture drawn alone, read back from the target
	Texel ShadeAloneIntoOutput(TestRenderer& test, TestTarget& target, GPUTextureHandle output, GPUMeshHandle quad, GPUTextureHandle texture)
	{
		FramePacket packet = MakePacket(quad, { { texture, AtDepth(0.5f) } });
		packet.OutputTarget = output;
		RenderFrame(test, target, packet);

		const Texel colour = SampleRenderTarget(test, output).front();
		CDL_CHECK_MSG(colour != kCleared, "a full-screen quad drew nothing into the output target, so nothing here can be compared");
		return colour;
	}

	// The scene shades in the scene pass and the GUI in its own; this is a packet whose scene is one quad of texture
	FramePacket MakeScenePacket(GPUMeshHandle quad, GPUTextureHandle texture)
	{
		FramePacket packet = MakePacket(quad, { { texture, AtDepth(0.5f) } });
		packet.GuiDrawData = MakeGuiDrawData(kDisplaySize);
		return packet;
	}

	// A packet with no scene, so the scene pass only clears and whatever isn't kCleared came from the GUI
	FramePacket MakeGuiOnlyPacket(glm::vec2 displayPos = { 0.0f, 0.0f })
	{
		FramePacket packet;
		packet.GuiDrawData = MakeGuiDrawData(kDisplaySize, displayPos);
		return packet;
	}

	// ImGui's colours are sRGB and its alpha linear coverage; Gui.slang decodes the colour, since the swapchain encodes on write
	float SrgbToLinear(uint8_t value)
	{
		const float c = value / 255.0f;
		return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
	}

	uint8_t ToUnorm(float value) { return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); }

}

// The far quad is drawn last in one order, where without depth testing it would cover the near one.
CDL_TEST_CASE(Renderer, NearerDrawHidesFartherWhicheverIsDrawnLast, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUMeshHandle quad = test.CreateQuad();
		const GPUTextureHandle nearTexture = test.CreateSolidTexture(kBright);
		const GPUTextureHandle farTexture = test.CreateSolidTexture(kDim);

		const Texel nearColour = ShadeAlone(test, target, quad, nearTexture);
		CheckDistinguishable(nearColour, ShadeAlone(test, target, quad, farTexture));

		const Layer nearLayer{ nearTexture, AtDepth(0.25f) };
		const Layer farLayer{ farTexture, AtDepth(0.75f) };
		ExpectEveryTexel(RenderFrame(test, target, MakePacket(quad, { nearLayer, farLayer })), nearColour, "far drawn last");
		ExpectEveryTexel(RenderFrame(test, target, MakePacket(quad, { farLayer, nearLayer })), nearColour, "near drawn last");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The compare is LessOrEqual, so a draw at exactly the stored depth passes and covers what is there, as a second pass over
// the same geometry needs. With Less, the first draw would win.
CDL_TEST_CASE(Renderer, LaterDrawWinsAtEqualDepth, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUMeshHandle quad = test.CreateQuad();
		const GPUTextureHandle first = test.CreateSolidTexture(kBright);
		const GPUTextureHandle second = test.CreateSolidTexture(kDim);

		const Texel secondColour = ShadeAlone(test, target, quad, second);
		CheckDistinguishable(ShadeAlone(test, target, quad, first), secondColour);

		const FramePacket packet = MakePacket(quad, { { first, AtDepth(0.5f) }, { second, AtDepth(0.5f) } });
		ExpectEveryTexel(RenderFrame(test, target, packet), secondColour, "second of two at equal depth");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Two half-screen quads side by side, so the left columns show only the first item and the right only the second. Fails if
// any item but the first is skipped, or if every item reads the same transform.
CDL_TEST_CASE(Renderer, DrawsEveryItemWithItsOwnTransform, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUMeshHandle quad = test.CreateQuad();
		const GPUTextureHandle left = test.CreateSolidTexture(kBright);
		const GPUTextureHandle right = test.CreateSolidTexture(kDim);

		const Texel leftColour = ShadeAlone(test, target, quad, left);
		const Texel rightColour = ShadeAlone(test, target, quad, right);
		CheckDistinguishable(leftColour, rightColour);

		// Halved in x, then centred on x = -0.5 or 0.5; the seam at x = 0 lies between pixel centres, so no pixel straddles it
		const glm::mat4 halfWidth = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 1.0f, 1.0f));
		const FramePacket packet = MakePacket(quad, {
			{ left, glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0.0f, 0.5f)) * halfWidth },
			{ right, glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 0.5f)) * halfWidth } });
		const std::vector<Texel> texels = RenderFrame(test, target, packet);

		for (uint32_t y = 0; y < kExtent.height; ++y)
		{
			for (uint32_t x = 0; x < kExtent.width; ++x)
			{
				const Texel& expected = x < kExtent.width / 2 ? leftColour : rightColour;
				ExpectTexelNear(texels[y * kExtent.width + x], expected, std::format("pixel ({}, {})", x, y), 0);
			}
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Built for a small target, then grown as RenderManager grows it on a resize; not square, so a swapped width and height
// shows. A depth image smaller than the render area fails validation, and the pixels show depth testing holds across all of it.
CDL_TEST_CASE(Renderer, RecreateTargetsSizesDepthToTheNewTarget, Gpu)
{
	SkipUnlessRendererShadersCompiled();
	constexpr vk::Extent2D kGrownExtent{ 16, 8 };

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		const GPUMeshHandle quad = test.CreateQuad();
		const GPUTextureHandle nearTexture = test.CreateSolidTexture(kBright);
		const GPUTextureHandle farTexture = test.CreateSolidTexture(kDim);

		TestTarget small = CreateTarget(test.Device, kExtent);
		const Texel nearColour = ShadeAlone(test, small, quad, nearTexture);
		CheckDistinguishable(nearColour, ShadeAlone(test, small, quad, farTexture));

		// Every frame was waited on, as RenderManager idles the GPU before it rebuilds
		test.SceneRenderer.RecreateTargets(kGrownExtent, test.Allocator);
		TestTarget grown = CreateTarget(test.Device, kGrownExtent);

		const FramePacket packet = MakePacket(quad, { { nearTexture, AtDepth(0.25f) }, { farTexture, AtDepth(0.75f) } });
		const std::vector<Texel> texels = RenderFrame(test, grown, packet);
		CDL_EXPECT_EQ(texels.size(), size_t{ kGrownExtent.width } * kGrownExtent.height);
		ExpectEveryTexel(texels, nearColour, "far drawn last after growing");
	}
	ExpectNoVulkanDebugMessages(logs);
}

//////////////////////////////////////////////////////////////////////////
// The GUI pass, drawing over the scene on the target
//////////////////////////////////////////////////////////////////////////

// The GUI pass loads what the scene pass drew rather than clearing it, so the scene shows wherever the GUI doesn't cover.
CDL_TEST_CASE(Renderer, GuiDrawsOverTheScene, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUMeshHandle quad = test.CreateQuad();
		const GPUTextureHandle sceneTexture = test.CreateSolidTexture(kBright);
		const GPUTextureHandle guiTexture = test.CreateSolidTexture(kGreen);
		const Texel sceneColour = ShadeAlone(test, target, quad, sceneTexture);
		CheckDistinguishable(sceneColour, kGreen);

		FramePacket packet = MakeScenePacket(quad, sceneTexture);
		AddGuiQuad(packet.GuiDrawData, { 0.0f, 0.0f }, { 2.0f, 4.0f }, guiTexture);

		std::vector<Texel> expected = Solid(sceneColour);
		Paint(expected, { 0, 0 }, { 2, 4 }, kGreen);
		ExpectImage(RenderFrame(test, target, packet), expected, "GUI over the left half");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// ImGui's coordinates run from DisplayPos with y down, as Vulkan's framebuffer does, so unlike the scene pass the GUI pass
// doesn't flip its viewport. The quad is off-centre on both axes, so a flip or a swapped axis lands it somewhere else.
CDL_TEST_CASE(Renderer, GuiQuadLandsWhereImGuiPlacedIt, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUTextureHandle guiTexture = test.CreateSolidTexture(kGreen);

		// Not at the origin, as a display that isn't the primary monitor's wouldn't be
		const glm::vec2 displayPos{ 100.0f, 50.0f };
		FramePacket packet = MakeGuiOnlyPacket(displayPos);
		AddGuiQuad(packet.GuiDrawData, displayPos + glm::vec2(1.0f, 0.0f), displayPos + glm::vec2(3.0f, 2.0f), guiTexture);

		std::vector<Texel> expected = Solid(kCleared);
		Paint(expected, { 1, 0 }, { 3, 2 }, kGreen);
		ExpectImage(RenderFrame(test, target, packet), expected, "quad from (1, 0) to (3, 2)");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The clip rect, already in framebuffer pixels, becomes the scissor, so a quad over the whole display shows only inside it.
CDL_TEST_CASE(Renderer, ClipRectScissorsTheGui, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUTextureHandle guiTexture = test.CreateSolidTexture(kGreen);

		FramePacket packet = MakeGuiOnlyPacket();
		AddGuiQuad(packet.GuiDrawData, { 0.0f, 0.0f }, kDisplaySize, guiTexture, kWhite, glm::vec4(0.0f, 1.0f, 3.0f, 4.0f));

		std::vector<Texel> expected = Solid(kCleared);
		Paint(expected, { 0, 1 }, { 3, 4 }, kGreen);
		ExpectImage(RenderFrame(test, target, packet), expected, "clipped to (0, 1) to (3, 4)");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A window dragged partly off the display has a clip rect running past the framebuffer. A scissor can't start below zero,
// and one clamped to nothing would come out inverted, its extent wrapping to billions, so the first are clamped and the
// rest skipped. Validation fails on a bad scissor, and the pixels show what each command really drew.
CDL_TEST_CASE(Renderer, ClipRectsOutsideTheTargetAreClampedOrSkipped, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUTextureHandle green = test.CreateSolidTexture(kGreen);
		const GPUTextureHandle blue = test.CreateSolidTexture(kBlue);
		const GPUTextureHandle red = test.CreateSolidTexture(kRed);

		FramePacket packet = MakeGuiOnlyPacket();
		GuiDrawData& gui = packet.GuiDrawData;
		AddGuiQuad(gui, { 0.0f, 0.0f }, kDisplaySize, green, kWhite, glm::vec4(-3.0f, -3.0f, 1.0f, 2.0f));	// Off the top left
		AddGuiQuad(gui, { 0.0f, 0.0f }, kDisplaySize, blue, kWhite, glm::vec4(3.0f, 3.0f, 100.0f, 100.0f));	// Off the bottom right
		AddGuiQuad(gui, { 0.0f, 0.0f }, kDisplaySize, red, kWhite, glm::vec4(5.0f, 0.0f, 9.0f, 4.0f));		// Wholly to the right
		AddGuiQuad(gui, { 0.0f, 0.0f }, kDisplaySize, red, kWhite, glm::vec4(2.0f, 2.0f, 2.0f, 4.0f));		// No width

		std::vector<Texel> expected = Solid(kCleared);
		Paint(expected, { 0, 0 }, { 1, 2 }, kGreen);
		Paint(expected, { 3, 3 }, { 4, 4 }, kBlue);
		ExpectImage(RenderFrame(test, target, packet), expected, "clamped and skipped clip rects");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// ImGui's vertex colours are sRGB with R in the low byte. Gui.slang decodes them, since the sRGB swapchain encodes on write,
// and multiplies the texture by them. The channels differ, so a byte-order mistake shows, and the right half's texture
// isn't white, so replacing it rather than multiplying shows too. Expected values come from the sRGB formula, not the shader.
CDL_TEST_CASE(Renderer, GuiVertexColourIsDecodedFromSrgbAndTintsTheTexture, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		constexpr Texel kGrey{ 128, 128, 128, 255 };
		constexpr Texel kLeftVertex{ 255, 188, 0, 255 };
		constexpr Texel kRightVertex{ 188, 255, 188, 255 };

		FramePacket packet = MakeGuiOnlyPacket();
		AddGuiQuad(packet.GuiDrawData, { 0.0f, 0.0f }, { 2.0f, 4.0f }, test.CreateSolidTexture(kWhite), kLeftVertex);
		AddGuiQuad(packet.GuiDrawData, { 2.0f, 0.0f }, { 4.0f, 4.0f }, test.CreateSolidTexture(kGrey), kRightVertex);

		// Opaque, so blending leaves the shaded colour as it is
		const auto shade = [](const Texel& texture, const Texel& vertex) {
			Texel shaded;
			for (size_t channel = 0; channel < 3; ++channel)
				shaded[channel] = ToUnorm(texture[channel] / 255.0f * SrgbToLinear(vertex[channel]));
			shaded[3] = 255;
			return shaded;
		};

		std::vector<Texel> expected = Solid(kCleared);
		Paint(expected, { 0, 0 }, { 2, 4 }, shade(kWhite, kLeftVertex));
		Paint(expected, { 2, 0 }, { 4, 4 }, shade(kGrey, kRightVertex));
		ExpectImage(RenderFrame(test, target, packet), expected, "tinted GUI", 2);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The GUI pipeline alpha-blends by the vertex alpha, which is coverage and so never sRGB-decoded: a transparent quad leaves
// the scene as it was, and a half-covered one mixes the two evenly.
CDL_TEST_CASE(Renderer, GuiBlendsOverTheSceneByVertexAlpha, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUMeshHandle quad = test.CreateQuad();
		const GPUTextureHandle sceneTexture = test.CreateSolidTexture(kBright);
		const GPUTextureHandle white = test.CreateSolidTexture(kWhite);
		const Texel sceneColour = ShadeAlone(test, target, quad, sceneTexture);

		constexpr uint8_t kHalfCoverage = 128;
		FramePacket packet = MakeScenePacket(quad, sceneTexture);
		AddGuiQuad(packet.GuiDrawData, { 0.0f, 0.0f }, { 2.0f, 4.0f }, white, Texel{ 255, 255, 255, 0 });
		AddGuiQuad(packet.GuiDrawData, { 2.0f, 0.0f }, { 4.0f, 4.0f }, white, Texel{ 255, 255, 255, kHalfCoverage });

		// BlendMode::AlphaBlend: colour is src * srcAlpha + dst * (1 - srcAlpha), alpha is srcAlpha + dstAlpha * (1 - srcAlpha)
		const float alpha = kHalfCoverage / 255.0f;
		Texel mixed;
		for (size_t channel = 0; channel < mixed.size(); ++channel)
			mixed[channel] = ToUnorm(alpha + sceneColour[channel] / 255.0f * (1.0f - alpha));

		std::vector<Texel> expected = Solid(sceneColour);
		Paint(expected, { 2, 0 }, { 4, 4 }, mixed);
		ExpectImage(RenderFrame(test, target, packet), expected, "transparent left, half-covered right", 2);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// ExtractDrawData gives each command its draw list's first vertex plus ImGui's VtxOffset, and ImGui's indices count from
// there: within a list they reach any of its vertices, and each list restarts them. Each command draws its own column, so
// a wrong vertex offset, index offset or texture puts the wrong colour in one.
CDL_TEST_CASE(Renderer, DrawsEachGuiCommandFromItsOwnOffsets, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUTextureHandle green = test.CreateSolidTexture(kGreen);
		const GPUTextureHandle blue = test.CreateSolidTexture(kBlue);
		const GPUTextureHandle red = test.CreateSolidTexture(kRed);

		FramePacket packet = MakeGuiOnlyPacket();
		GuiDrawData& gui = packet.GuiDrawData;
		const glm::vec4 wholeDisplay(0.0f, 0.0f, kDisplaySize);

		// One draw list of two quads, both commands counting from the list's first vertex
		const uint32_t firstList = AddGuiQuadVertices(gui, { 0.0f, 0.0f }, { 1.0f, 4.0f });
		const uint32_t secondQuad = AddGuiQuadVertices(gui, { 1.0f, 0.0f }, { 2.0f, 4.0f });
		AddGuiQuadCommand(gui, green, firstList, firstList, wholeDisplay);
		AddGuiQuadCommand(gui, blue, firstList, secondQuad, wholeDisplay);

		// A second list after it, counting from its own first vertex
		const uint32_t secondList = AddGuiQuadVertices(gui, { 2.0f, 0.0f }, { 4.0f, 4.0f });
		AddGuiQuadCommand(gui, red, secondList, secondList, wholeDisplay);

		std::vector<Texel> expected = Solid(kRed);
		Paint(expected, { 0, 0 }, { 1, 4 }, kGreen);
		Paint(expected, { 1, 0 }, { 2, 4 }, kBlue);
		ExpectImage(RenderFrame(test, target, packet), expected, "one column per command");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A frame slot keeps its GUI buffers between frames: they grow to at least double, so a growing UI doesn't reallocate
// every frame, and are written again as they are while the GUI fits.
CDL_TEST_CASE(Renderer, GuiBuffersGrowByDoublingAndAreReusedWhileTheGuiFits, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUTextureHandle white = test.CreateSolidTexture(kWhite);
		constexpr vk::DeviceSize kQuadVertexBytes = 4 * sizeof(GPU::GuiVertex);
		constexpr vk::DeviceSize kQuadIndexBytes = 6 * sizeof(GuiIndex);

		const auto withQuads = [&](int count) {
			FramePacket packet = MakeGuiOnlyPacket();
			for (int i = 0; i < count; ++i)
				AddGuiQuad(packet.GuiDrawData, { 0.0f, 0.0f }, kDisplaySize, white);
			return packet;
		};

		RenderFrame(test, target, withQuads(2));
		CDL_EXPECT_EQ(test.Frame.GuiVertices.GetSize(), 2 * kQuadVertexBytes);
		CDL_EXPECT_EQ(test.Frame.GuiIndices.GetSize(), 2 * kQuadIndexBytes);

		// Three quads need half as much again, so doubling, not the need, sets the new size
		RenderFrame(test, target, withQuads(3));
		CDL_EXPECT_EQ(test.Frame.GuiVertices.GetSize(), 4 * kQuadVertexBytes);
		CDL_EXPECT_EQ(test.Frame.GuiIndices.GetSize(), 4 * kQuadIndexBytes);
		const vk::DeviceAddress grownVertices = test.Frame.GuiVertices.GetDeviceAddress();
		const vk::Buffer grownIndices = test.Frame.GuiIndices.GetBuffer();

		RenderFrame(test, target, withQuads(1));
		CDL_EXPECT_EQ(test.Frame.GuiVertices.GetDeviceAddress(), grownVertices);
		CDL_EXPECT(test.Frame.GuiIndices.GetBuffer() == grownIndices);
		CDL_EXPECT_EQ(test.Frame.GuiVertices.GetSize(), 4 * kQuadVertexBytes);
	}
	ExpectNoVulkanDebugMessages(logs);
}

//////////////////////////////////////////////////////////////////////////
// Rendering to an output target, which the GUI shows
//////////////////////////////////////////////////////////////////////////

// The scene goes to the output target, depth-tested as on the swapchain, and with no GUI the target is only cleared: in
// the editor the scene shows nowhere but in the window that samples it.
CDL_TEST_CASE(Renderer, RenderToTextureDrawsTheSceneIntoTheOutputTarget, Gpu)
{
	SkipUnlessRendererShadersCompiled();
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent, RenderTo::OutputTarget);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUTextureHandle output = test.CreateOutputTarget({ kExtent.width, kExtent.height });
		const GPUMeshHandle quad = test.CreateQuad();
		const GPUTextureHandle nearTexture = test.CreateSolidTexture(kBright);
		const GPUTextureHandle farTexture = test.CreateSolidTexture(kDim);

		const Texel nearColour = ShadeAloneIntoOutput(test, target, output, quad, nearTexture);
		CheckDistinguishable(nearColour, ShadeAloneIntoOutput(test, target, output, quad, farTexture));

		FramePacket packet = MakePacket(quad, { { nearTexture, AtDepth(0.25f) }, { farTexture, AtDepth(0.75f) } });
		packet.OutputTarget = output;
		ExpectEveryTexel(RenderFrame(test, target, packet), kCleared, "target with no GUI");
		ExpectEveryTexel(SampleRenderTarget(test, output), nearColour, "output target, far drawn last");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The editor's scene window: the GUI samples the output target the scene pass wrote earlier in the same frame. The target
// held a different shade before, so a GUI pass that ran first, or read stale contents, shows the wrong one.
CDL_TEST_CASE(Renderer, GuiShowsTheOutputTargetTheSceneDrewThatFrame, Gpu)
{
	SkipUnlessRendererShadersCompiled();
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent, RenderTo::OutputTarget);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUTextureHandle output = test.CreateOutputTarget({ kExtent.width, kExtent.height });
		const GPUMeshHandle quad = test.CreateQuad();
		const GPUTextureHandle bright = test.CreateSolidTexture(kBright);
		const GPUTextureHandle dim = test.CreateSolidTexture(kDim);

		const Texel brightColour = ShadeAloneIntoOutput(test, target, output, quad, bright);
		CheckDistinguishable(brightColour, ShadeAloneIntoOutput(test, target, output, quad, dim));	// Leaves the dim shade in the target

		FramePacket packet = MakeScenePacket(quad, bright);
		packet.OutputTarget = output;
		AddGuiQuad(packet.GuiDrawData, { 0.0f, 0.0f }, { 2.0f, 4.0f }, output);

		std::vector<Texel> expected = Solid(kCleared);
		Paint(expected, { 0, 0 }, { 2, 4 }, brightColour);
		ExpectImage(RenderFrame(test, target, packet), expected, "scene window over the left half");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Depth starts at the renderer's target extent, and RenderManager matches it to each output target before drawing. The new
// target is larger, so depth left at the old size fails validation, and not square, so a swapped width and height shows.
CDL_TEST_CASE(Renderer, MatchDepthToOutputSizesDepthToTheOutputTarget, Gpu)
{
	SkipUnlessRendererShadersCompiled();
	SkipUnlessCompiled(kBindlessFixturePath);
	constexpr glm::uvec2 kOutputExtent{ 16, 8 };

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent, RenderTo::OutputTarget);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUMeshHandle quad = test.CreateQuad();
		const GPUTextureHandle nearTexture = test.CreateSolidTexture(kBright);
		const GPUTextureHandle farTexture = test.CreateSolidTexture(kDim);

		const GPUTextureHandle small = test.CreateOutputTarget({ kExtent.width, kExtent.height });
		const Texel nearColour = ShadeAloneIntoOutput(test, target, small, quad, nearTexture);
		CheckDistinguishable(nearColour, ShadeAloneIntoOutput(test, target, small, quad, farTexture));

		// Every frame was waited on, so the old depth image is already free to go
		const GPUTextureHandle grown = test.CreateOutputTarget(kOutputExtent);
		test.SceneRenderer.MatchDepthToOutput(kOutputExtent, test.Device.GetCompletedTimelineValue(QueueType::Graphics));

		FramePacket packet = MakePacket(quad, { { nearTexture, AtDepth(0.25f) }, { farTexture, AtDepth(0.75f) } });
		packet.OutputTarget = grown;
		RenderFrame(test, target, packet);

		const std::vector<Texel> texels = SampleRenderTarget(test, grown);
		CDL_EXPECT_EQ(texels.size(), size_t{ kOutputExtent.x } * kOutputExtent.y);
		ExpectEveryTexel(texels, nearColour, "far drawn last after growing");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// RenderManager calls it every frame, so the common case, a target the size depth already is, must neither allocate a new
// depth image nor queue the current one.
CDL_TEST_CASE(Renderer, MatchDepthToOutputKeepsDepthWhenTheExtentIsUnchanged, Gpu)
{
	SkipUnlessRendererShadersCompiled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent, RenderTo::OutputTarget);
		const uint32_t before = LiveAllocations(test.Allocator);

		test.SceneRenderer.MatchDepthToOutput({ kExtent.width, kExtent.height }, std::numeric_limits<uint64_t>::max());
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), before);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Frames in flight may still test against the depth image MatchDepthToOutput replaces, so it lives until the GPU reaches the
// value it was given, the newest frame's, and the first Render after that frees it.
CDL_TEST_CASE(Renderer, ReplacedDepthImageIsFreedOnceItsLastFrameRetires, Gpu)
{
	SkipUnlessRendererShadersCompiled();
	constexpr glm::uvec2 kGrownExtent{ 8, 8 };

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent, RenderTo::OutputTarget);
		TestTarget target = CreateTarget(test.Device, kExtent);
		FramePacket packet = MakePacket(test.CreateQuad(), { { test.CreateSolidTexture(kBright), AtDepth(0.5f) } });
		packet.OutputTarget = test.CreateOutputTarget({ kExtent.width, kExtent.height });
		RenderFrame(test, target, packet);

		packet.OutputTarget = test.CreateOutputTarget(kGrownExtent);
		const uint32_t before = LiveAllocations(test.Allocator);

		// Every submit so far has been waited on, so the GPU reaches this with the next one: the frame below
		const uint64_t lastUse = test.Device.GetCompletedTimelineValue(QueueType::Graphics) + 1;
		test.SceneRenderer.MatchDepthToOutput(kGrownExtent, lastUse);
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), before + 1u);	// The new depth image, and the old one held

		RenderFrame(test, target, packet);	// Drains before its own submit, so before lastUse is reached
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), before + 1u);

		RenderFrame(test, target, packet);	// Drains with lastUse reached
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), before);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A swapchain resize says nothing about the output target, so depth stays at the target's size: nothing is reallocated,
// and a frame at the old output extent still depth-tests, with the swapchain image now larger than it.
CDL_TEST_CASE(Renderer, RecreateTargetsLeavesDepthAloneWhenRenderingToAnOutputTarget, Gpu)
{
	SkipUnlessRendererShadersCompiled();
	SkipUnlessCompiled(kBindlessFixturePath);
	constexpr vk::Extent2D kGrownExtent{ 16, 8 };

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent, RenderTo::OutputTarget);
		TestTarget small = CreateTarget(test.Device, kExtent);
		const GPUTextureHandle output = test.CreateOutputTarget({ kExtent.width, kExtent.height });
		const GPUMeshHandle quad = test.CreateQuad();
		const GPUTextureHandle nearTexture = test.CreateSolidTexture(kBright);
		const GPUTextureHandle farTexture = test.CreateSolidTexture(kDim);

		const Texel nearColour = ShadeAloneIntoOutput(test, small, output, quad, nearTexture);
		CheckDistinguishable(nearColour, ShadeAloneIntoOutput(test, small, output, quad, farTexture));

		// A check, not an expectation: Render asserts that depth matches the output target, which would end the whole run
		const uint32_t before = LiveAllocations(test.Allocator);
		test.SceneRenderer.RecreateTargets(kGrownExtent, test.Allocator);
		CDL_CHECK_EQ(LiveAllocations(test.Allocator), before);

		TestTarget grown = CreateTarget(test.Device, kGrownExtent);
		FramePacket packet = MakePacket(quad, { { nearTexture, AtDepth(0.25f) }, { farTexture, AtDepth(0.75f) } });
		packet.OutputTarget = output;
		ExpectEveryTexel(RenderFrame(test, grown, packet), kCleared, "grown target with no GUI");
		ExpectEveryTexel(SampleRenderTarget(test, output), nearColour, "output target after the swapchain grew");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Debug draws go through DebugDrawManager as a game's do, so these also check its colour packing against DebugDraw.slang.
// View and projection stay identity, so debug vertices are in clip space. The viewport flips y but not x, so the lines
// run down columns, which land in the same place whichever way up the image is.
namespace {

	constexpr vk::Extent2D kWideExtent{ 16, 16 };

	float ColumnCentre(uint32_t column, uint32_t width) { return -1.0f + (column + 0.5f) * 2.0f / width; }

	// Past both edges, so the line's own ends never land inside the target
	void DrawColumn(uint32_t column, uint32_t width, float depth, glm::vec4 colour, const DebugDraw::Options& options = {})
	{
		const float x = ColumnCentre(column, width);
		DebugDraw::Line({ x, -2.0f, depth }, { x, 2.0f, depth }, colour, options);
	}

	uint32_t CountDrawnColumns(std::span<const Texel> texels, vk::Extent2D extent, uint32_t row, uint32_t firstColumn, uint32_t endColumn)
	{
		uint32_t drawn = 0;
		for (uint32_t x = firstColumn; x < endColumn; ++x)
			drawn += texels[row * extent.width + x] != kCleared;
		return drawn;
	}

}

// Every channel differs, so any mismatch between how DebugDrawManager packs the colour and how the shader unpacks it shows.
// Each value is a multiple of 1/15, so it survives the four bits a channel exactly.
CDL_TEST_CASE(Renderer, DebugLineDrawsInTheColourItWasGiven, Gpu)
{
	SkipUnlessRendererShadersCompiled();
	SkipUnlessDebugDrawEnabled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		ConsoleManager console;
		DebugDrawManager debugDraw(console);

		DrawColumn(1, kExtent.width, 0.5f, { 1.0f, 0.6f, 0.2f, 0.8f });
		FramePacket packet;
		debugDraw.ExtractDebugDrawData(packet.DebugDraw, 0.0f);
		const std::vector<Texel> texels = RenderFrame(test, target, packet);

		const Texel expected{ 255, 153, 51, 204 };
		for (uint32_t row = 0; row < kExtent.height; ++row)
		{
			const Texel drawn = texels[row * kExtent.width + 1];
			CDL_EXPECT_MSG(drawn == expected, std::format("row {} of the line is {}, expected {}", row, drawn, expected));
		}
		CDL_EXPECT_EQ(CountDrawnColumns(texels, kExtent, 1, 0, kExtent.width), 1u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The scene's quad is nearer than both lines, so only the one skipping the depth test shows
CDL_TEST_CASE(Renderer, OnlyDebugLinesSkippingTheDepthTestShowThroughTheScene, Gpu)
{
	SkipUnlessRendererShadersCompiled();
	SkipUnlessDebugDrawEnabled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUMeshHandle quad = test.CreateQuad();
		const GPUTextureHandle sceneTexture = test.CreateSolidTexture(kBright);
		const Texel scene = ShadeAlone(test, target, quad, sceneTexture);

		ConsoleManager console;
		DebugDrawManager debugDraw(console);
		const glm::vec4 green{ 0.0f, 1.0f, 0.0f, 1.0f };
		DrawColumn(0, kExtent.width, 0.75f, green, { .DepthTest = true });
		DrawColumn(3, kExtent.width, 0.75f, green, { .DepthTest = false });

		FramePacket packet = MakePacket(quad, { { sceneTexture, AtDepth(0.25f) } });
		debugDraw.ExtractDebugDrawData(packet.DebugDraw, 0.0f);
		const std::vector<Texel> texels = RenderFrame(test, target, packet);

		const uint32_t row = kExtent.height / 2;
		CDL_EXPECT_MSG(texels[row * kExtent.width + 0] == scene, "the depth tested line drew over the nearer scene");
		CDL_EXPECT_MSG(texels[row * kExtent.width + 3] != scene, "the line skipping the depth test was hidden by the scene");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Both widths are in one frame, so this fails if the width isn't set per batch, as when the last one set wins for all
CDL_TEST_CASE(Renderer, EachDebugBatchDrawsAtItsOwnLineWidth, Gpu)
{
	SkipUnlessRendererShadersCompiled();
	SkipUnlessDebugDrawEnabled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kWideExtent);
		TestTarget target = CreateTarget(test.Device, kWideExtent);
		ConsoleManager console;
		DebugDrawManager debugDraw(console);

		const glm::vec4 white{ 1.0f };
		DrawColumn(3, kWideExtent.width, 0.5f, white, { .LineWidth = 1.0f });
		DrawColumn(11, kWideExtent.width, 0.5f, white, { .LineWidth = 5.0f });

		FramePacket packet;
		debugDraw.ExtractDebugDrawData(packet.DebugDraw, 0.0f);
		CDL_CHECK_EQ(packet.DebugDraw.Batches.size(), 2u);
		const std::vector<Texel> texels = RenderFrame(test, target, packet);

		const uint32_t row = kWideExtent.height / 2;
		CDL_EXPECT_EQ(CountDrawnColumns(texels, kWideExtent, row, 0, kWideExtent.width / 2), 1u);
		CDL_EXPECT_EQ(CountDrawnColumns(texels, kWideExtent, row, kWideExtent.width / 2, kWideExtent.width), 5u);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The GUI pass runs after the debug draw pass and loads what it drew, so the GUI covers the lines under it and no others.
// White packs to all ones, so these don't depend on the channel order the colour test checks.
CDL_TEST_CASE(Renderer, GuiDrawsOverDebugLines, Gpu)
{
	SkipUnlessRendererShadersCompiled();
	SkipUnlessDebugDrawEnabled();

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUTextureHandle guiTexture = test.CreateSolidTexture(kGreen);
		ConsoleManager console;
		DebugDrawManager debugDraw(console);

		DrawColumn(1, kExtent.width, 0.5f, glm::vec4(1.0f));
		DrawColumn(3, kExtent.width, 0.5f, glm::vec4(1.0f));
		FramePacket packet = MakeGuiOnlyPacket();
		AddGuiQuad(packet.GuiDrawData, { 0.0f, 0.0f }, { 2.0f, 4.0f }, guiTexture);
		debugDraw.ExtractDebugDrawData(packet.DebugDraw, 0.0f);

		std::vector<Texel> expected = Solid(kCleared);
		Paint(expected, { 3, 0 }, { 4, 4 }, kWhite);
		Paint(expected, { 0, 0 }, { 2, 4 }, kGreen);
		ExpectImage(RenderFrame(test, target, packet), expected, "GUI over the left half, debug lines in columns 1 and 3");
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The editor's scene window shows the output target rather than the swapchain, so debug draws have to land there with the scene
CDL_TEST_CASE(Renderer, DebugLinesDrawIntoTheOutputTarget, Gpu)
{
	SkipUnlessRendererShadersCompiled();
	SkipUnlessDebugDrawEnabled();
	SkipUnlessCompiled(kBindlessFixturePath);

	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestRenderer test(kExtent, RenderTo::OutputTarget);
		TestTarget target = CreateTarget(test.Device, kExtent);
		const GPUTextureHandle output = test.CreateOutputTarget({ kExtent.width, kExtent.height });
		ConsoleManager console;
		DebugDrawManager debugDraw(console);

		DrawColumn(1, kExtent.width, 0.5f, glm::vec4(1.0f));
		FramePacket packet;
		packet.OutputTarget = output;
		debugDraw.ExtractDebugDrawData(packet.DebugDraw, 0.0f);
		RenderFrame(test, target, packet);

		std::vector<Texel> expected = Solid(kCleared);
		Paint(expected, { 1, 0 }, { 2, kExtent.height }, kWhite);
		ExpectImage(SampleRenderTarget(test, output), expected, "output target, debug line in column 1");
	}
	ExpectNoVulkanDebugMessages(logs);
}
