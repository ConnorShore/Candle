#pragma once

// Shared setup for the Vulkan-backed tests. Only test files under Tests/Render include Vulkan.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "BindlessFixture.h"

#include <Candle/Render/BindlessTable.h>
#include <Candle/Render/FramePacket.h>
#include <Candle/Render/GeometryPool.h>
#include <Candle/Render/GPUAllocator.h>
#include <Candle/Render/GPUBuffer.h>
#include <Candle/Render/GPUResourceRegistry.h>
#include <Candle/Render/GPUUploader.h>
#include <Candle/Render/Image.h>
#include <Candle/Render/Pipeline.h>
#include <Candle/Render/PipelineSpecification.h>
#include <Candle/Render/RenderDevice.h>
#include <Candle/Render/RenderInstance.h>
#include <Candle/Render/RenderSpecification.h>
#include <Candle/Render/SamplerType.h>
#include <Candle/Render/ShaderCreationInfo.h>
#include <Candle/Render/ShaderInterop/Vertex.h>
#include <Candle/Render/ShaderLoader.h>
#include <Candle/Render/TextureData.h>
#include <Candle/Render/TexturePool.h>

#include <vk_mem_alloc.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Candle::Test {

	// Validation on, reporting warnings and errors: a correct program produces neither. Set explicitly, so
	// the tests don't lean on the default, which DefaultSpecForwardsValidationErrors checks on its own.
	inline RenderSpecification StrictRenderSpec()
	{
		RenderSpecification spec;
		spec.ValidationSpec.EnableValidation = true;
		spec.ValidationSpec.Severity = ValidationSpecification::ValidationSeverity::Warning;
		return spec;
	}

	inline RenderInstanceSpecification TestInstanceSpec(const RenderSpecification& renderSpec)
	{
		return {
			.VulkanVersion = renderSpec.VulkanVersion,
			.ValidationSpec = renderSpec.ValidationSpec,
			.ApplicationName = "Candle-Test",
		};
	}

	// The debug messenger logs on the Render channel; pass to LoggerFixture to capture only that.
	inline constexpr uint16_t kRenderChannelMask = static_cast<uint16_t>(LogChannel::Render);

	// The debug messenger logs at the message's own severity, so match its prefix rather than a level: Render
	// also warns about non-Vulkan things. Flushes first, so call it after the Vulkan objects under test are
	// destroyed to include their teardown messages.
	inline std::vector<std::string> VulkanDebugMessages(LoggerFixture& logs)
	{
		CDL_CHECK(Logger::Flush());

		std::vector<std::string> messages;
		for (size_t i = 0; i < logs.Records.size(); ++i)
		{
			const LogRecord& record = logs.Records[i];
			if (record.Channel == LogChannel::Render && !record.IsContinuation && std::string_view(record.Message).starts_with("[Vulkan Debug]"))
				messages.push_back(JoinedMessage(logs.Records, i));
		}
		return messages;
	}

	// Notes every message, so a validation error names its VUID in the test output.
	inline void ExpectNoVulkanDebugMessages(LoggerFixture& logs)
	{
		const std::vector<std::string> messages = VulkanDebugMessages(logs);
		for (const std::string& message : messages)
			CDL_NOTE(message);
		CDL_EXPECT_EQ(messages.size(), 0u);
	}

	// Members construct in order and destroy in reverse, so the device goes before the instance and the backend.
	struct TestDevice
	{
		WindowingBackend Backend;
		RenderSpecification Spec = StrictRenderSpec();
		RenderInstance Instance{ TestInstanceSpec(Spec) };
		RenderDevice Device{ Instance, Spec };
	};

	// A device and the VMA allocator over it. The base is destroyed last, so the allocator goes before the device.
	struct TestAllocator : TestDevice
	{
		GPUAllocator Allocator{ { .VulkanVersion = Spec.VulkanVersion, .Device = &Device, .Instance = &Instance } };
	};

	// What every pool needs, in dependency order: it stages through the uploader, which allocates through the allocator.
	struct TestPoolBase : TestAllocator
	{
		GPUUploader Uploader{ Device, Allocator };
		GPUResourceRegistry Registry;
	};

	// Uploads don't block, so the GPU is drained before the pool frees the meshes it still holds.
	struct TestGeometry : TestPoolBase
	{
		GeometryPool Pool{ Allocator, Uploader, Registry };

		inline ~TestGeometry() { Device.WaitIdle(); }
	};

	// TexturePool plus the set-0 table its views are published in, declared first so it outlives the textures, as in
	// RenderManager. Drained like TestGeometry, since a texture's upload may still be running.
	struct TestTextures : TestPoolBase
	{
		BindlessTable Bindless{ Device };
		TexturePool Pool{ Allocator, Uploader, Registry, Bindless };

		// Drains the defaults' staging so a test counts only its own allocations
		inline TestTextures()
		{
			Device.WaitIdle();
			Uploader.FreeCompletedStagingBuffers();
		}

		inline ~TestTextures() { Device.WaitIdle(); }
	};

	// The images every TexturePool holds from construction to destruction, one per DefaultTexture
	inline constexpr uint32_t kDefaultTextureAllocations = static_cast<uint32_t>(DefaultTexture::Count);

	// Stands in for one frame of RenderManager: pending releases ride a packet to an empty submit, retire with it once
	// the GPU reaches it, and their slots come back to the main thread. Frees the uploads' staging too.
	inline void RunReleaseFrame(TestPoolBase& gpu)
	{
		FramePacket packet;
		gpu.Registry.MoveReleasedSlotsIntoPacket(packet);

		const uint64_t retireValue = gpu.Device.Submit(QueueType::Graphics, {});
		gpu.Registry.RetireReleasedSlots(packet, retireValue);
		gpu.Device.WaitFor(QueueType::Graphics, retireValue);
		gpu.Registry.FreeRetiredSlots(gpu.Device.GetCompletedTimelineValue(QueueType::Graphics), packet);

		gpu.Registry.ReturnRecycledSlotsToPools(packet);
		gpu.Uploader.FreeCompletedStagingBuffers();
	}

	// One triangle and its indices, for tests that need any mesh at all.
	inline const std::array<GPU::Vertex, 3> kTriangleVertices = { {
		{ { 0.5f, -0.5f, 0.0f } },
		{ { 0.0f, 0.5f, 0.0f } },
		{ { -0.5f, -0.5f, 0.0f } } } };
	inline constexpr std::array<uint32_t, 3> kTriangleIndices = { 0, 1, 2 };

	// Every allocation VMA holds right now, so a test can see a buffer freed, or leaked.
	inline uint32_t LiveAllocations(GPUAllocator& allocator)
	{
		VmaTotalStatistics stats{};
		vmaCalculateStatistics(allocator.GetAllocator(), &stats);
		return stats.total.statistics.allocationCount;
	}

	// Host-visible with random access, since the test reads it back; a transfer destination so the GPU can fill it.
	inline GPUBuffer CreateHostBuffer(GPUAllocator& allocator, vk::DeviceSize size)
	{
		return allocator.CreateBuffer({
			.Size = size,
			.Usage = vk::BufferUsageFlagBits::eTransferDst,
			.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT });
	}

	// Invalidates first, in case the memory is not host-coherent.
	inline std::vector<std::byte> ReadBack(GPUAllocator& allocator, const GPUBuffer& buffer)
	{
		std::vector<std::byte> bytes(buffer.GetSize());
		CDL_CHECK(vmaCopyAllocationToMemory(allocator.GetAllocator(), buffer.GetAllocation(), 0, bytes.data(), bytes.size()) == VK_SUCCESS);
		return bytes;
	}

	// Distinct, recognisable bytes: seed, seed + 1, ... wrapping at 256.
	inline std::vector<std::byte> BytePattern(size_t count, uint8_t seed)
	{
		std::vector<std::byte> bytes(count);
		for (size_t i = 0; i < count; ++i)
			bytes[i] = static_cast<std::byte>(static_cast<uint8_t>(seed + i));
		return bytes;
	}

	// One member of an interop struct as C++ lays it out, to compare against a shader's reflection.
	struct InteropMember
	{
		std::string_view Name;
		uint32_t Offset;
		uint32_t Size;
	};
#define CDL_INTEROP_MEMBER(Type, Member) \
	::Candle::Test::InteropMember{ #Member, static_cast<uint32_t>(offsetof(Type, Member)), static_cast<uint32_t>(sizeof(Type::Member)) }

	// Fails unless the reflected block holds exactly these members of T, at the offsets and sizes C++ gives them.
	// Reflection ends a block at its last member while sizeof rounds up to alignof, so T's tail padding is allowed.
	template<typename T>
	void ExpectPushBlockMatches(const PushConstantBlock& block, std::span<const InteropMember> members)
	{
		const size_t paddedSize = (block.Size + alignof(T) - 1) / alignof(T) * alignof(T);
		CDL_EXPECT_EQ(paddedSize, sizeof(T));
		CDL_CHECK_EQ(block.Members.size(), members.size());
		for (size_t i = 0; i < members.size(); ++i)
		{
			const PushConstantMember& reflected = block.Members[i];
			CDL_EXPECT_MSG(reflected.Name == members[i].Name, std::format("member {} is '{}', expected '{}'", i, reflected.Name, members[i].Name));
			CDL_EXPECT_MSG(reflected.Offset == members[i].Offset,
				std::format("{} is at {} in the shader, {} in C++", members[i].Name, reflected.Offset, members[i].Offset));
			CDL_EXPECT_MSG(reflected.Size == members[i].Size,
				std::format("{} is {} bytes in the shader, {} in C++", members[i].Name, reflected.Size, members[i].Size));
		}
	}

	// One pool and primary buffer on the graphics family, for tests that record their own commands.
	// Reset the pool only once the GPU has finished with the buffer.
	struct TestCommands
	{
		vk::raii::CommandPool Pool{ nullptr };
		vk::raii::CommandBuffer Buffer{ nullptr };

		explicit TestCommands(RenderDevice& device)
		{
			vk::CommandPoolCreateInfo poolInfo{ .queueFamilyIndex = device.GetQueueFamily(QueueType::Graphics) };
			Pool = vk::raii::CommandPool(device.GetDevice(), poolInfo);

			vk::CommandBufferAllocateInfo allocInfo{ .commandPool = *Pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
			Buffer = std::move(vk::raii::CommandBuffers(device.GetDevice(), allocInfo).front());
		}
	};

	// Submits one command buffer on the graphics queue and blocks until the GPU has run it.
	inline void SubmitAndWait(RenderDevice& device, const vk::raii::CommandBuffer& commandBuffer)
	{
		const vk::CommandBufferSubmitInfo commandBufferInfo{ .commandBuffer = *commandBuffer };
		device.WaitFor(QueueType::Graphics, device.Submit(QueueType::Graphics, { .CommandBuffers = { &commandBufferInfo, 1 } }));
	}

	// A VkImage the test owns, for code that takes one it doesn't own, as with swapchain images.
	// Memory is declared first so the image is destroyed before the memory it is bound to.
	struct OwnedImage
	{
		vk::raii::DeviceMemory Memory{ nullptr };
		vk::raii::Image Handle{ nullptr };
	};

	inline uint32_t FindDeviceLocalMemoryType(RenderDevice& device, uint32_t allowedTypes)
	{
		const vk::PhysicalDeviceMemoryProperties properties = device.GetPhysicalDevice().getMemoryProperties();
		for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
		{
			if ((allowedTypes & (1u << i)) && (properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal))
				return i;
		}

		CDL_CHECK_MSG(false, "no device-local memory type fits the image");
		return 0;
	}

	// 2D, one layer, optimally tiled, bound to device-local memory and still in Undefined
	inline OwnedImage CreateOwnedImage(RenderDevice& device, vk::Format format, vk::Extent2D extent, uint32_t mipLevels, vk::ImageUsageFlags usage)
	{
		OwnedImage image;
		image.Handle = vk::raii::Image(device.GetDevice(), vk::ImageCreateInfo{
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = { extent.width, extent.height, 1 },
			.mipLevels = mipLevels,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = usage,
			.initialLayout = vk::ImageLayout::eUndefined });

		const vk::MemoryRequirements requirements = image.Handle.getMemoryRequirements();
		image.Memory = vk::raii::DeviceMemory(device.GetDevice(), vk::MemoryAllocateInfo{
			.allocationSize = requirements.size,
			.memoryTypeIndex = FindDeviceLocalMemoryType(device, requirements.memoryTypeBits) });
		image.Handle.bindMemory(*image.Memory, 0);
		return image;
	}

	// Compiled shaders are build outputs, not checked-in files, so a missing one is a setup step rather than a failure.
	inline void SkipUnlessCompiled(const std::filesystem::path& spirvPath)
	{
		const auto file = Platform::ReadFile(spirvPath);
		if (!file && file.error().Code == FileErrorCode::NotFound)
			CDL_SKIP(std::format("{} is not compiled; run Build.py shaders", spirvPath.string()));
	}

	inline SharedPtr<Shader> LoadTestShader(const std::string& name, const std::filesystem::path& spirvPath)
	{
		auto shader = ShaderLoader::LoadShader(name, spirvPath);
		if (!shader)
			CDL_CHECK_MSG(false, shader.error());
		return *shader;
	}

	// One RGBA8 texel, the only layout TexturePool takes.
	using Texel = std::array<uint8_t, 4>;
	inline constexpr Texel kRed{ 255, 0, 0, 255 };
	inline constexpr Texel kGreen{ 0, 255, 0, 255 };
	inline constexpr Texel kBlue{ 0, 0, 255, 255 };
	inline constexpr Texel kWhite{ 255, 255, 255, 255 };

	// A binary PPM: stb_image decodes it, and unlike PNG it needs no encoder to write. RGB only, so alpha is dropped.
	inline std::filesystem::path WritePPM(const TempDirectory& dir, std::string_view name, glm::uvec2 extent, std::span<const Texel> texels)
	{
		CDL_CHECK_EQ(texels.size(), static_cast<size_t>(extent.x) * extent.y);

		const std::string header = std::format("P6\n{} {}\n255\n", extent.x, extent.y);
		std::vector<std::byte> file(header.size());
		std::memcpy(file.data(), header.data(), header.size());
		for (const Texel& texel : texels)
			for (size_t channel = 0; channel < 3; ++channel)
				file.push_back(static_cast<std::byte>(texel[channel]));

		const std::filesystem::path path = dir / name;
		CDL_CHECK(Platform::WriteFile(path, file).has_value());
		return path;
	}

	// CreateTexture also publishes the view at its slot of set 0, so the texture is ready to sample
	inline GPUTextureHandle CreateBoundTexture(TestTextures& gpu, const TextureCreateSpec& spec)
	{
		return gpu.Pool.CreateTexture(spec);
	}

	// RGBA8, and linear by default, so a sampled texel comes back as the bytes that went in.
	inline GPUTextureHandle CreateBoundTexture(TestTextures& gpu, glm::uvec2 extent, std::span<const Texel> texels,
		TextureColorSpace colorSpace = TextureColorSpace::Linear)
	{
		return CreateBoundTexture(gpu, { .Data = std::as_bytes(texels), .Extent = extent, .Format = TextureFormat::Rgba8, .ColorSpace = colorSpace });
	}

	inline constexpr vk::ImageSubresourceRange kColorSubresourceRange{
		.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 };

	inline constexpr ImageState kTransferSrcState{
		.Layout = vk::ImageLayout::eTransferSrcOptimal,
		.Access = vk::AccessFlagBits2::eTransferRead,
		.Stage = vk::PipelineStageFlagBits2::eCopy };

	inline constexpr ImageState kColorAttachmentState{
		.Layout = vk::ImageLayout::eColorAttachmentOptimal,
		.Access = vk::AccessFlagBits2::eColorAttachmentWrite,
		.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput };

	// Copies a 4-byte-per-texel colour image to the host, tightly packed, once the GPU has run everything before it.
	// `from` is the state the GPU's last use left it in. Needs TransferSrc usage, and leaves the image in TransferSrcOptimal.
	inline std::vector<std::byte> ReadBackImage(TestAllocator& gpu, vk::Image image, vk::Extent2D extent, const ImageState& from)
	{
		GPUBuffer readback = CreateHostBuffer(gpu.Allocator, vk::DeviceSize{ extent.width } * extent.height * sizeof(Texel));
		TestCommands commands(gpu.Device);
		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		const vk::ImageMemoryBarrier2 toTransferSrc = MakeImageBarrier(image, from, kTransferSrcState, kColorSubresourceRange);
		commands.Buffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toTransferSrc });

		const vk::BufferImageCopy2 region{
			.imageSubresource = { .aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
			.imageExtent = { extent.width, extent.height, 1 } };
		commands.Buffer.copyImageToBuffer2({ .srcImage = image, .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
			.dstBuffer = readback.GetBuffer(), .regionCount = 1, .pRegions = &region });

		// The host reads the buffer next, once SubmitAndWait returns
		const vk::MemoryBarrier2 toHost{
			.srcStageMask = vk::PipelineStageFlagBits2::eCopy, .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eHost, .dstAccessMask = vk::AccessFlagBits2::eHostRead };
		commands.Buffer.pipelineBarrier2({ .memoryBarrierCount = 1, .pMemoryBarriers = &toHost });
		commands.Buffer.end();

		SubmitAndWait(gpu.Device, commands.Buffer);
		return ReadBack(gpu.Allocator, readback);
	}

	inline std::vector<Texel> ToTexels(std::span<const std::byte> bytes)
	{
		std::vector<Texel> texels(bytes.size() / sizeof(Texel));
		std::memcpy(texels.data(), bytes.data(), texels.size() * sizeof(Texel));
		return texels;
	}

	// Filtering and sRGB decoding round differently across GPUs, so a sampled colour may be a step or two off.
	inline void ExpectTexelNear(const Texel& sampled, const Texel& expected, std::string_view label, int tolerance = 2)
	{
		bool near = true;
		for (size_t channel = 0; channel < sampled.size(); ++channel)
			near = near && std::abs(int{ sampled[channel] } - int{ expected[channel] }) <= tolerance;
		CDL_EXPECT_MSG(near, std::format("{}: sampled {}, expected {}", label, sampled, expected));
	}

	// Relative to the repo root, which is where Build.py test and the Visual Studio debugger both run from.
	inline const std::filesystem::path kBindlessFixturePath = "Candle-Test/res/shaders/bin/BindlessFixture.spv";

	// One sample as a shader takes it: the texture's slot of set 0, through one of the bindless samplers, at UV.
	struct TextureProbe
	{
		GPUTextureHandle Texture;
		SamplerType Sampler;
		glm::vec2 UV;
	};

	// Draws each probe into its own pixel of a probes.size() x 1 target with BindlessFixture and reads the colours back,
	// so a test sees what a shader really samples. The probed textures must already be written into gpu.Bindless.
	inline std::vector<Texel> SampleTextures(TestTextures& gpu, std::span<const TextureProbe> probes)
	{
		constexpr vk::Format kTargetFormat = vk::Format::eR8G8B8A8Unorm;	// UNORM, so the shader's result is stored unconverted
		const vk::Extent2D extent{ static_cast<uint32_t>(probes.size()), 1 };

		Pipeline pipeline(PipelineSpecification{
			.Shader = LoadTestShader("BindlessFixture", kBindlessFixturePath),
			.ColorAttachmentFormats = { kTargetFormat },
			.CullBackFaces = false }, gpu.Device, gpu.Bindless);
		vk::PipelineLayout layout = *pipeline.GetLayout();

		OwnedImage target = CreateOwnedImage(gpu.Device, kTargetFormat, extent, 1,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
		vk::raii::ImageView view(gpu.Device.GetDevice(), vk::ImageViewCreateInfo{
			.image = *target.Handle, .viewType = vk::ImageViewType::e2D, .format = kTargetFormat, .subresourceRange = kColorSubresourceRange });

		TestCommands commands(gpu.Device);
		commands.Buffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		const vk::ImageMemoryBarrier2 toAttachment = MakeImageBarrier(*target.Handle, {}, kColorAttachmentState, kColorSubresourceRange);
		commands.Buffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toAttachment });

		// Cleared, so a probe that never drew reads back as transparent black rather than as whatever the memory held
		const vk::RenderingAttachmentInfo attachment{
			.imageView = *view, .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear, .storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f) };
		commands.Buffer.beginRendering({ .renderArea = { { 0, 0 }, extent }, .layerCount = 1, .colorAttachmentCount = 1, .pColorAttachments = &attachment });
		commands.Buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.GetPipeline());
		gpu.Bindless.BindSet(*commands.Buffer, vk::PipelineBindPoint::eGraphics, layout);
		commands.Buffer.setScissor(0, vk::Rect2D({ 0, 0 }, extent));

		for (uint32_t i = 0; i < extent.width; ++i)
		{
			commands.Buffer.setViewport(0, vk::Viewport(static_cast<float>(i), 0.0f, 1.0f, 1.0f, 0.0f, 1.0f));
			pipeline.PushConstants(*commands.Buffer, GPU::BindlessFixturePushConstants{
				.UV = probes[i].UV,
				.TextureIndex = gpu.Pool.GetTextureInfo(probes[i].Texture).Index,
				.SamplerIndex = static_cast<uint32_t>(probes[i].Sampler) });
			commands.Buffer.draw(3, 1, 0, 0);
		}

		commands.Buffer.endRendering();
		commands.Buffer.end();
		SubmitAndWait(gpu.Device, commands.Buffer);

		return ToTexels(ReadBackImage(gpu, *target.Handle, extent, kColorAttachmentState));
	}

}
