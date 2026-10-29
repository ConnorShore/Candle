// Renderer on a real device, drawing packets into an image the test owns in place of a swapchain image and reading the
// pixels back, so a test sees what a frame really shows. Each test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "RenderTestHelpers.h"

#include <Candle/Render/FrameContext.h>
#include <Candle/Render/FramePacket.h>
#include <Candle/Render/Renderer.h>
#include <Candle/Render/ShaderInterop/FrameData.h>
#include <Candle/Render/SwapChainImage.h>

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	// Renderer loads the engine's compiled triangle, a build output rather than a checked-in file.
	const std::filesystem::path kTriangleShaderPath = "Candle/res/shaders/bin/Triangle.spv";

	constexpr vk::Format kTargetFormat = vk::Format::eR8G8B8A8Unorm;	// UNORM, so the shader's result is stored unconverted
	constexpr vk::Extent2D kExtent{ 4, 4 };
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

	// What RenderManager holds around its Renderer, in the same order, plus one frame slot. Each frame is waited on but the
	// mesh and texture uploads are not, so the GPU is drained before the pools go.
	struct TestRenderer : TestTextures
	{
		GeometryPool Geometry{ Allocator, Uploader, Registry };
		Renderer SceneRenderer;
		FrameContext Frame = CreateFrameContext(*this);

		explicit TestRenderer(vk::Extent2D extent)
			: SceneRenderer(Device, Geometry, Pool, Bindless, Allocator, kTargetFormat, extent) {}
		~TestRenderer() { Device.WaitIdle(); }

		GPUMeshHandle CreateQuad() { return Geometry.CreateMesh(kQuadVertices, kQuadIndices); }
		GPUTextureHandle CreateSolidTexture(const Texel& colour) { return CreateBoundTexture(*this, { 1, 1 }, std::array<Texel, 1>{ colour }); }
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

}

// The far quad is drawn last in one order, where without depth testing it would cover the near one.
CDL_TEST_CASE(Renderer, NearerDrawHidesFartherWhicheverIsDrawnLast, Gpu)
{
	SkipUnlessCompiled(kTriangleShaderPath);

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
	SkipUnlessCompiled(kTriangleShaderPath);

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
	SkipUnlessCompiled(kTriangleShaderPath);

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
	SkipUnlessCompiled(kTriangleShaderPath);
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
