#include "cdlpch.h"
#include "Renderer.h"

#include "Candle/Render/Vulkan/RenderDevice.h"
#include "Candle/Render/Pipelines/PipelineSpecification.h"
#include "Candle/Render/FramePacket.h"
#include "Candle/Render/FrameContext.h"
#include "Candle/Render/Vulkan/SwapChainImage.h"
#include "Candle/Render/Resources/GeometryPool.h"
#include "Candle/Render/Resources/TexturePool.h"
#include "Candle/Render/Vulkan/GPUAllocator.h"
#include "Candle/Render/RenderConstants.h"
#include "Candle/Render/GUI/GuiDrawData.h"
#include "Candle/Render/GUI/GuiVertex.h"

// TODO: Probably should live in pipeline but see once render graph is made
#include "Candle/Render/GUI/GuiInterop.h"
#include "Candle/Render/Pipelines/TriangleInterop.h"
#include "Candle/Render/Pipelines/DebugDrawInterop.h"

#include "Candle/Render/Pipelines/ShaderLoader.h"

#include <cmath>
#include <span>

namespace Candle {

	namespace {
		// A float time steps ~0.24 ms at an hour; wrapping keeps shader animation smooth for one jump per period
		constexpr double kShaderTimeWrapSeconds = 3600.0;

		constexpr vk::ImageSubresourceRange kColorRange{ .aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1 };

		// A color attachment the next pass only writes, since its clear replaces whatever was there
		constexpr ImageState kColorAttachmentWrite{
			.Layout = vk::ImageLayout::eColorAttachmentOptimal,
			.Access = vk::AccessFlagBits2::eColorAttachmentWrite,
			.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput };

		// A color attachment the next pass also reads: its load and blending read what the pass before it wrote
		constexpr ImageState kColorAttachmentReadWrite{
			.Layout = vk::ImageLayout::eColorAttachmentOptimal,
			.Access = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
			.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput };

		// Storage images are only ever written in General
		constexpr ImageState kComputeStorageWrite{
			.Layout = vk::ImageLayout::eGeneral,
			.Access = vk::AccessFlagBits2::eShaderStorageWrite,
			.Stage = vk::PipelineStageFlagBits2::eComputeShader };

		// A texture the next pass samples in its fragment shader, in the layout its bindless descriptor was written with
		ImageState FragmentSampled(const TextureInfo& texture)
		{
			return { .Layout = texture.SampledLayout, .Access = vk::AccessFlagBits2::eShaderSampledRead, .Stage = vk::PipelineStageFlagBits2::eFragmentShader };
		}

		vk::Format FindDepthFormat(RenderDevice& device)
		{
			// Depth-only, so the depth-aspect view and barrier stay valid without separateDepthStencilLayouts. The spec requires
			// D16 and one of the first two as attachments, so the panic below can't be reached on a conformant device.
			const std::vector<vk::Format> preferredFormats = {
				vk::Format::eD32Sfloat,
				vk::Format::eX8D24UnormPack32,
				vk::Format::eD16Unorm
			};
			for (const auto& format : preferredFormats)
			{
				vk::FormatProperties formatProperties = device.GetPhysicalDevice().getFormatProperties(format);
				if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
					return format;
			}
			CDL_CORE_PANIC(LogChannel::Render, "No suitable depth format found");
		}

		// The device reports its range whether or not wideLines is enabled, but without it only 1 is valid
		glm::vec2 FindLineWidthRange(RenderDevice& device)
		{
			if (!device.HasWideLines())
				return { 1.0f, 1.0f };

			const auto& range = device.GetPhysicalDevice().getProperties().limits.lineWidthRange;
			return { range[0], range[1] };
		}

		GPUImageSpec DepthImageSpec(vk::Format format, glm::uvec2 extent)
		{
			return { .Extent = extent,
				.Format = format,
				.Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
				.Aspect = vk::ImageAspectFlagBits::eDepth,
				.DebugName = "Depth Target" };
		}

		// The renderer can't draw anything without its own shaders, so a missing or bad .spv is fatal
		// TODO: Temporary until the asset manager loads shaders
		SharedPtr<Shader> LoadEngineShader(const std::string& name, const std::filesystem::path& filePath)
		{
			auto shader = ShaderLoader::LoadShader(name, filePath);
			if (!shader)
				CDL_CORE_PANIC(LogChannel::Render, "{}", shader.error());

			return std::move(*shader);
		}

		// TODO: Eventually move out of here once render grpah is implemented
		template<typename T>
		void ApplyPushConstants(vk::CommandBuffer commandBuffer, vk::PipelineLayout layout, const T& data)
		{
			CDL_STATIC_ASSERT(std::is_same_v<decltype(T::Global), GPU::GlobalPushConstants> && offsetof(T, Global) == 0,
				"A push-constant struct must start with 'GlobalPushConstants Global'");
			CDL_STATIC_ASSERT(sizeof(T) <= kPushConstantsSize, "Push-constant struct exceeds the engine-wide range");
			commandBuffer.pushConstants(layout, kPushConstantStages, 0, sizeof(T), &data);
		}
	}

	Renderer::Renderer(RenderDevice& renderDevice, GeometryPool& geometryPool, TexturePool& texturePool, BindlessTable& bindlessTable,
		GPUAllocator& allocator, const RendererCreationInfo& info)
		: m_RenderDevice(renderDevice)
		, m_GeometryPool(geometryPool)
		, m_TexturePool(texturePool)
		, m_BindlessTable(bindlessTable)
		, m_GPUAllocator(allocator)
		, m_PipelineStore(renderDevice, bindlessTable)
		, m_RenderToSwapChain(info.RenderToSwapChain)
		, m_TargetFormat(info.TargetFormat)
		, m_DepthFormat(FindDepthFormat(renderDevice))
		, m_DepthImage(DepthImageSpec(m_DepthFormat, { info.TargetExtent.width, info.TargetExtent.height }), allocator) // Sized to the colour target
		, m_DepthExtent(info.TargetExtent.width, info.TargetExtent.height)
		, m_LineWidthRange(FindLineWidthRange(renderDevice))
	{
		// Load shaders into pipeline for now 
		// TODO: Eventually will be part of the asset manager's initialization and will use asset handles rather than pointers
		m_TrianglePipeline = m_PipelineStore.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = LoadEngineShader("TrianglePipeline", info.ShaderDirectory / "Triangle.spv"),
			.VertexEntryPoint = "VertMain",
			.FragmentEntryPoint = "FragMain",
			.ColorAttachmentFormats = { info.TargetFormat },
			.DepthAttachmentFormat = m_DepthFormat,
			.DebugName = "Triangle Pipeline"
		});
		m_DebugDrawPipeline = m_PipelineStore.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = LoadEngineShader("DebugDrawPipeline", info.ShaderDirectory / "DebugDraw.spv"),
			.VertexEntryPoint = "VertMain",
			.FragmentEntryPoint = "FragMain",
			.Topology = PrimitiveTopology::LineList,
			.ColorAttachmentFormats = { info.TargetFormat },
			.DepthAttachmentFormat = m_DepthFormat,
			.DebugName = "Debug Draw Pipeline"
		});
		m_GuiPipeline = m_PipelineStore.CreatePipeline(GraphicsPipelineSpecification{
			.Shader = LoadEngineShader("GuiPipeline", info.ShaderDirectory / "Gui.spv"),
			.VertexEntryPoint = "VertMain",
			.FragmentEntryPoint = "FragMain",
			.Blend = BlendMode::AlphaBlend,
			.ColorAttachmentFormats = { info.TargetFormat },
			.CullBackFaces = false,
			.DebugName = "GUI Pipeline"
		});

		m_ComputeTex = m_TexturePool.CreateRenderTarget({
			.Extent{ glm::uvec2(64, 64) },
			.Format{ vk::Format::eR8G8B8A8Unorm },
			.Usage{ vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled },
			.DebugName{ "Test Compute Target" }
			});

		// Test Compute shader that will perform operations to be used by the triangle pipeline. This is just a test for now, will be removed later
		m_ComputePipeline = m_PipelineStore.CreatePipeline(ComputePipelineSpecification{
			.Shader = LoadEngineShader("ComputePipeline", info.ShaderDirectory / "Triangle.spv"),
			.EntryPoint = "CompMain",
			.DebugName = "Test Compute Pipeline"
			});
	}

	void Renderer::Render(const FramePacket& packet, FrameContext& frame, SwapChainImage& target)
	{
		CDL_PROFILE_SCOPE("Renderer::Render");

		// TODO: The render graph will own the passes, their pipelines, the depth target and the barriers between them;
		//	until then the passes are recorded here by hand

		// Frees the depth images MatchDepthToOutput replaced, once the frames that used them have retired
		m_RetiredImages.Drain(QueueType::Graphics, m_RenderDevice.GetCompletedTimelineValue(QueueType::Graphics));

		// Write the transform data into the frame's TransformData buffer, if any
		std::span<const glm::mat4> transforms = packet.TransformData;
		if (transforms.size() > kMaxTransforms)
		{
			// Checked in every config, since the write below would run past the end of the buffer
			CDL_CORE_ASSERT(false, "FramePacket has more transforms than kMaxTransforms");
			transforms = transforms.first(kMaxTransforms);
		}

		if (!transforms.empty())
			frame.TransformData.Write(std::as_bytes(transforms));

		// Safe to overwrite: RenderManager waited on this slot's RetireValue, and vkQueueSubmit makes host writes visible to the GPU
		const GPU::FrameData frameData{ 
			.ViewProjection = packet.RenderView.Projection * packet.RenderView.View,
			.TransformData = frame.TransformData.GetDeviceAddress(),
			.TransformCount = static_cast<uint32_t>(transforms.size()),
			.Time = static_cast<float>(std::fmod(packet.Time, kShaderTimeWrapSeconds))
		};
		frame.FrameData.Write(std::as_bytes(std::span(&frameData, 1)));

		const GPU::GlobalPushConstants global{ 
			.Frame = frame.FrameData.GetDeviceAddress()
		};

		// Begin recording commands into the command buffer
		frame.CommandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		// Bind the global descriptor set once, which contains the bindless table and the frame data buffer
		m_BindlessTable.BindSet(*frame.CommandBuffer, vk::PipelineBindPoint::eGraphics, m_PipelineStore.GetGlobalLayout());
		m_BindlessTable.BindSet(*frame.CommandBuffer, vk::PipelineBindPoint::eCompute, m_PipelineStore.GetGlobalLayout());

		// Record the compute pipeline
		RecordComputePipeline(packet, frame, global);

		if (m_RenderToSwapChain)
		{
			// The scene pass clears the target, so it runs every frame, even with nothing to draw
			target.Transition(kColorAttachmentWrite, frame.CommandBuffer);
			RenderTrianglePipeline(packet, frame, { .ColorView = target.GetImageView(), .Extent = target.GetDesc().Extent }, global);

			// The debug draw pass draws over the scene; the tracked state makes this transition order it after the scene's writes
			target.Transition(kColorAttachmentReadWrite, frame.CommandBuffer);
			RenderDebugDrawPipeline(packet, frame, { .ColorView = target.GetImageView(), .Extent = target.GetDesc().Extent }, vk::AttachmentLoadOp::eLoad, global);

			// The GUI pass draws over the scene; the tracked state makes this transition order it after the scene's writes
			target.Transition(kColorAttachmentReadWrite, frame.CommandBuffer);
			RenderGuiPipeline(packet, frame, target, vk::AttachmentLoadOp::eLoad, global);	// Returns early with no GUI
		}
		else
		{
			const TextureInfo output = m_TexturePool.GetTextureInfo(packet.OutputTarget);
			if (output.Usage & vk::ImageUsageFlagBits::eColorAttachment)
			{
				CDL_CORE_ASSERT(output.Extent == m_DepthExtent, "Depth must match the output target; RenderManager calls MatchDepthToOutput first");

				// Undefined discards last frame's contents, which the clear replaces. Last frame's GUI pass sampled it, and a write
				// after a read needs only an execution dependency, so the source has a stage but no access
				const vk::ImageMemoryBarrier2 toAttachment = MakeImageBarrier(output.Image,
					{ .Layout = vk::ImageLayout::eUndefined, .Access = vk::AccessFlagBits2::eNone, .Stage = vk::PipelineStageFlagBits2::eFragmentShader },
					kColorAttachmentWrite, kColorRange);
				frame.CommandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toAttachment });

				const SceneTarget sceneTarget{ .ColorView = output.ImageView, .Extent = { output.Extent.x, output.Extent.y } };
				RenderTrianglePipeline(packet, frame, sceneTarget, global);

				// The debug pass loads what the scene pass wrote, so it has to wait for those writes
				const vk::ImageMemoryBarrier2 toDebugDraw = MakeImageBarrier(output.Image, kColorAttachmentWrite, kColorAttachmentReadWrite, kColorRange);
				frame.CommandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toDebugDraw });
				RenderDebugDrawPipeline(packet, frame, sceneTarget, vk::AttachmentLoadOp::eLoad, global);

				// The GUI samples it next
				const vk::ImageMemoryBarrier2 toSampled = MakeImageBarrier(output.Image, kColorAttachmentWrite, FragmentSampled(output), kColorRange);
				frame.CommandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toSampled });
			}
			else
				CDL_CORE_ASSERT(false, "Not rendering to the swapchain, but the packet has no output target to render the scene into");

			// Nothing else draws to the target, so the GUI pass clears it, and runs even with no GUI to draw
			target.Transition(kColorAttachmentWrite, frame.CommandBuffer);
			RenderGuiPipeline(packet, frame, target, vk::AttachmentLoadOp::eClear, global);
		}

		// Every pass leaves the target as a color attachment, and presentation reads it next
		target.Transition({
			.Layout = vk::ImageLayout::ePresentSrcKHR,
			.Access = vk::AccessFlagBits2::eNone
			}, frame.CommandBuffer);

		// End recording commands into the command buffer. After this, the command buffer can be submitted to the graphics queue for execution.
		frame.CommandBuffer.end();
	}

	void Renderer::RecreateTargets(vk::Extent2D targetExtent, GPUAllocator& allocator)
	{
		// Otherwise depth follows the output target, which a swapchain resize doesn't change
		if (!m_RenderToSwapChain)
			return;

		m_DepthExtent = { targetExtent.width, targetExtent.height };
		m_DepthImage.Recreate(DepthImageSpec(m_DepthFormat, m_DepthExtent), allocator);
	}

	void Renderer::MatchDepthToOutput(glm::uvec2 outputExtent, uint64_t lastUseValue)
	{
		CDL_CORE_ASSERT(!m_RenderToSwapChain, "Depth follows the swapchain when rendering to it; RecreateTargets resizes it");
		if (outputExtent == m_DepthExtent)
			return;

		// Frames in flight may still test against the old image, so it waits in the deletion queue rather than being freed here
		m_RetiredImages.Enqueue(QueueType::Graphics, lastUseValue, std::move(m_DepthImage));
		m_DepthExtent = outputExtent;
		m_DepthImage = GPUImage(DepthImageSpec(m_DepthFormat, m_DepthExtent), m_GPUAllocator);
	}

	void Renderer::RenderTrianglePipeline(const FramePacket& packet, FrameContext& frame, const SceneTarget& target, const GPU::GlobalPushConstants globalPCs)
	{
		auto trianglePipeline = m_PipelineStore.TryGetPipeline(m_TrianglePipeline);
		if (!trianglePipeline)
		{
			CDL_CORE_ASSERT(false, "Triangle pipeline is not ready");
			return;
		}

		// Transition the depth image to DEPTH_ATTACHMENT_OPTIMAL
		vk::ImageMemoryBarrier2 depthBarrier = MakeImageBarrier(
			m_DepthImage.GetImage(),
			// Undefined discards the old contents, which the clear would overwrite anyway. Every frame in flight shares this one image,
			// so wait for the previous frame's depth writes (tests and store op), or this transition and clear race them
			{ .Layout = vk::ImageLayout::eUndefined,
				.Access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				.Stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests },
			// The clear writes in early tests, the depth compare reads and writes in early or late tests
			{ .Layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
				.Access = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				.Stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests },
			{ .aspectMask = vk::ImageAspectFlagBits::eDepth, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }		// The depth image is a single 2D image, so we only have one mip level and one array layer
		);
		frame.CommandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &depthBarrier });

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

		vk::RenderingAttachmentInfo colorAttachmentInfo = {
			.imageView = target.ColorView,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor };

		// The debug pass loads depth to test its lines against the scene, so it's only discarded when there are none
		vk::RenderingAttachmentInfo depthAttachmentInfo = {
			.imageView = m_DepthImage.GetImageView(),
			.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = packet.DebugDraw.Vertices.empty() ? vk::AttachmentStoreOp::eDontCare : vk::AttachmentStoreOp::eStore,
			.clearValue = clearDepth
		};

		const vk::Extent2D extent = target.Extent;

		// Setup the rendering info for the render pass begin info. This specifies the render area, the number of layers, and the color attachment info.
		vk::RenderingInfo renderingInfo = {
			.renderArea = {
				.offset = { 0, 0 },
				.extent = extent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo,
			.pDepthAttachment = &depthAttachmentInfo
		};

		{
			GPUDebugScope mainPass(m_RenderDevice, frame.CommandBuffer, "Main Triangle Pass");
			frame.CommandBuffer.beginRendering(renderingInfo);

			frame.CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, trianglePipeline);

			frame.CommandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(extent.height), static_cast<float>(extent.width), -static_cast<float>(extent.height), 0.0f, 1.0f));
			frame.CommandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
			frame.CommandBuffer.setDepthTestEnable(true);

			const uint32_t computeTextureIndex = m_TexturePool.GetTextureInfo(m_ComputeTex).Index;

			// Draw each item
			for (const DrawItem& item : packet.DrawItems)
			{
				// The GPU doesn't bounds-check a read through a device address, so skip a bad index in every config
				if (item.TransformIndex >= packet.TransformData.size())
				{
					CDL_CORE_ASSERT(false, "DrawItem's TransformIndex is past the packet's TransformData");
					continue;
				}

				MeshDrawInfo drawInfo = m_GeometryPool.GetMeshInfo(item.Mesh);
				TextureInfo textureInfo = m_TexturePool.GetTextureInfo(item.Texture);

				GPU::MeshDrawPushConstants pushConstants{
					.Global = globalPCs,
					.Vertices = drawInfo.VerticesAddr,
					.TextureIndex = textureInfo.Index,
					.SamplerIndex = static_cast<uint32_t>(item.Sampler),
					.TransformIndex = item.TransformIndex,
					.ComputeTextureIndex = computeTextureIndex
				};

				ApplyPushConstants(*frame.CommandBuffer, m_PipelineStore.GetGlobalLayout(), pushConstants);
				frame.CommandBuffer.bindIndexBuffer(drawInfo.Indices, 0, vk::IndexType::eUint32);
				frame.CommandBuffer.drawIndexed(drawInfo.IndexCount, 1, 0, 0, 0);
			}

			frame.CommandBuffer.endRendering();
		}
	}

	void Renderer::RenderDebugDrawPipeline(const FramePacket& packet, FrameContext& frame, const SceneTarget& target, vk::AttachmentLoadOp loadOp, const GPU::GlobalPushConstants globalPCs)
	{
		// Skip the pass if there's nothing to draw, so the target's loadOp is respected and the depth image isn't transitioned
		const DebugDrawData& debugDraw = packet.DebugDraw;
		if (debugDraw.Vertices.empty())
			return;

		auto debugDrawPipeline = m_PipelineStore.TryGetPipeline(m_DebugDrawPipeline);
		if (!debugDrawPipeline)
		{
			CDL_CORE_ASSERT(false, "Debug draw pipeline is not ready");
			return;
		}

		// Rendering instances don't order against each other, so the load and depth tests here wait on the scene pass's depth
		// writes and its store, which land in early or late tests. The layout doesn't change
		const vk::ImageMemoryBarrier2 depthBarrier = MakeImageBarrier(m_DepthImage.GetImage(),
			{ .Layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
				.Access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				.Stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests },
			{ .Layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
				.Access = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				.Stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests },
			{ .aspectMask = vk::ImageAspectFlagBits::eDepth, .levelCount = 1, .layerCount = 1 });
		frame.CommandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &depthBarrier });

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

		vk::RenderingAttachmentInfo colorAttachmentInfo{
			.imageView = target.ColorView,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = loadOp,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor };

		vk::RenderingAttachmentInfo depthAttachmentInfo{
			.imageView = m_DepthImage.GetImageView(),
			.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eLoad,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = clearDepth
		};

		const vk::Extent2D extent = target.Extent;

		// Setup the rendering info for the render pass begin info. This specifies the render area, the number of layers, and the color attachment info.
		vk::RenderingInfo renderingInfo{
			.renderArea = {
				.offset = { 0, 0 },
				.extent = extent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo,
			.pDepthAttachment = &depthAttachmentInfo
		};

		EnsureCapacity(frame.DebugDrawVertices, debugDraw.Vertices.size() * sizeof(GPU::DebugDrawVertex), vk::BufferUsageFlagBits::eShaderDeviceAddress, "DebugDrawVertices");
		frame.DebugDrawVertices.Write(std::as_bytes(std::span(debugDraw.Vertices)));

		{
			GPUDebugScope mainPass(m_RenderDevice, frame.CommandBuffer, "Debug Draw Pass");
			frame.CommandBuffer.beginRendering(renderingInfo);

			frame.CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, debugDrawPipeline);

			frame.CommandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(extent.height), static_cast<float>(extent.width), -static_cast<float>(extent.height), 0.0f, 1.0f));
			frame.CommandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
			
			const vk::DeviceAddress vertexBase = frame.DebugDrawVertices.GetDeviceAddress();

			std::optional<DebugDrawBucket> currentBucket;
			for (const DebugDrawBatch& batch : debugDraw.Batches)
			{
				if (batch.Bucket != currentBucket)
				{
					bool depthEnabled, worldViewProj;
					switch (batch.Bucket)
					{
					case DebugDrawBucket::WorldDepthTested:
						depthEnabled = true;
						worldViewProj = true;
						break;
					case DebugDrawBucket::WorldNoDepth:
						depthEnabled = false;
						worldViewProj = true;
						break;
					case DebugDrawBucket::Screen:
						depthEnabled = false;
						worldViewProj = false;
						break;
					default:
						CDL_CORE_ASSERT(false, "Unhandled bucket type");
						continue;
					}

					frame.CommandBuffer.setDepthTestEnable(depthEnabled);
					currentBucket = batch.Bucket;

					// TODO: Some way to use world ViewProj matrix or a screen space matrix
				}

				frame.CommandBuffer.setLineWidth(std::clamp(batch.LineWidth, m_LineWidthRange.x, m_LineWidthRange.y));

				GPU::DebugDrawPushConstants pushConstants{
					.Global = globalPCs,
					.Vertices = vertexBase + (batch.FirstVertex * sizeof(GPU::DebugDrawVertex))
				};

				ApplyPushConstants(*frame.CommandBuffer, m_PipelineStore.GetGlobalLayout(), pushConstants);
				frame.CommandBuffer.draw(batch.VertexCount, 1, 0, 0);
			}

			frame.CommandBuffer.endRendering();
		}
	}

	void Renderer::RenderGuiPipeline(const FramePacket& packet, FrameContext& frame, SwapChainImage& target, vk::AttachmentLoadOp loadOp, const GPU::GlobalPushConstants globalPCs)
	{
		const GuiDrawData& gui = packet.GuiDrawData;
		const bool hasDraws = !gui.Commands.empty();

		// With nothing to draw, the pass only matters for its clear
		if (!hasDraws && loadOp != vk::AttachmentLoadOp::eClear)
			return;

		auto guiPipeline = m_PipelineStore.TryGetPipeline(m_GuiPipeline);
		if (!guiPipeline)
		{
			CDL_CORE_ASSERT(false, "GUI pipeline is not ready");
			return;
		}

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);

		// Color attachment only, no depth. Loaded to draw over the scene, or cleared when the scene went to an output target
		vk::RenderingAttachmentInfo colorAttachmentInfo = {
			.imageView = target.GetImageView(),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = loadOp,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor };

		vk::Extent2D swapChainExtent = target.GetDesc().Extent;

		// Setup the rendering info for the render pass begin info. 
		// This specifies the render area, the number of layers, and the color attachment info.
		vk::RenderingInfo renderingInfo = {
			.renderArea = {
				.offset = { 0, 0 },
				.extent = swapChainExtent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo,
			.pDepthAttachment = nullptr
		};

		if (!hasDraws)
		{
			// Just the clear
			GPUDebugScope guiPass(m_RenderDevice, frame.CommandBuffer, "GUI Pass (clear only)");
			frame.CommandBuffer.beginRendering(renderingInfo);
			frame.CommandBuffer.endRendering();
			return;
		}

		// Write the GUI vertex and index data into the frame's GuiVertexData and GuiIndexData buffers
		EnsureCapacity(frame.GuiVertices, gui.Vertices.size() * sizeof(Candle::GPU::GuiVertex), vk::BufferUsageFlagBits::eShaderDeviceAddress, "GuiVertices");
		EnsureCapacity(frame.GuiIndices, gui.Indices.size() * sizeof(GuiIndex), vk::BufferUsageFlagBits::eIndexBuffer, "GuiIndices");
		frame.GuiVertices.Write(std::as_bytes(std::span(gui.Vertices)));
		frame.GuiIndices.Write(std::as_bytes(std::span(gui.Indices)));

		{
			GPUDebugScope guiPass(m_RenderDevice, frame.CommandBuffer, "GUI Pass");

			frame.CommandBuffer.beginRendering(renderingInfo);

			frame.CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, guiPipeline);

			// Not flipped: Vulkan's NDC already has Y pointing down, as ImGui's coordinates do
			frame.CommandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
			frame.CommandBuffer.setDepthTestEnable(false);

			// Maps ImGui's display rect [DisplayPos, DisplayPos + DisplaySize] to NDC [-1, 1]
			const glm::vec2 scale = 2.0f / gui.DisplaySize;
			const glm::vec2 translate = -1.0f - gui.DisplayPos * scale;

			// Bind the GUI vertex and index buffers
			frame.CommandBuffer.bindIndexBuffer(frame.GuiIndices.GetBuffer(), 0, vk::IndexType::eUint16);
			const vk::DeviceAddress vertexBase = frame.GuiVertices.GetDeviceAddress();

			// Draw each item
			uint32_t samplerIndex = static_cast<uint32_t>(SamplerType::LinearClamp);	// The GUI pipeline only uses one sampler
			for (const GuiDrawCommand& cmd : gui.Commands)
			{
				// The clip rect is already in framebuffer pixels; clamp it, since a scissor can't start below zero
				const glm::vec2 clipMin = glm::max(glm::vec2(cmd.ClipRect.x, cmd.ClipRect.y), glm::vec2(0.0f));
				const glm::vec2 clipMax = glm::min(glm::vec2(cmd.ClipRect.z, cmd.ClipRect.w), glm::vec2(swapChainExtent.width, swapChainExtent.height));
				if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
					continue;	// Entirely clipped

				frame.CommandBuffer.setScissor(0, vk::Rect2D(
					vk::Offset2D(static_cast<int32_t>(clipMin.x), static_cast<int32_t>(clipMin.y)),
					vk::Extent2D(static_cast<uint32_t>(clipMax.x - clipMin.x), static_cast<uint32_t>(clipMax.y - clipMin.y))));

				GPU::GuiDrawPushConstants pushConstants{
					.Global = globalPCs,
					.Vertices = vertexBase + cmd.VertexOffset * sizeof(Candle::GPU::GuiVertex),  // Offset here rather than as drawIndexed's vertexOffset, so the shader indexes from 0
					.Scale = scale,
					.Translate = translate,
					.TextureIndex = m_TexturePool.GetTextureInfo(cmd.Texture).Index,
					.SamplerIndex = samplerIndex
				};

				ApplyPushConstants(*frame.CommandBuffer, m_PipelineStore.GetGlobalLayout(), pushConstants);
				frame.CommandBuffer.drawIndexed(cmd.IndexCount, 1, cmd.IndexOffset, 0, 0);
			}

			frame.CommandBuffer.endRendering();
		}
	}

	void Renderer::RecordComputePipeline(const FramePacket& packet, FrameContext& frame, const GPU::GlobalPushConstants globalPCs)
	{
		auto computePipeline = m_PipelineStore.TryGetPipeline(m_ComputePipeline);
		if (!computePipeline)
		{
			CDL_CORE_ASSERT(false, "Compute pipeline is not ready");
			return;
		}

		auto compEntryPoint = m_PipelineStore.GetEntryPoint(m_ComputePipeline, ShaderStage::Compute);
		if (!compEntryPoint)
		{
			CDL_CORE_ASSERT(false, "Failed to get compute pipeline entry point: {}", compEntryPoint.error());
			return;
		}

		const TextureInfo target = m_TexturePool.GetTextureInfo(m_ComputeTex);

		// Undefined discards last frame's contents, which the dispatch replaces. Last frame's scene pass sampled it, and a write
		// after a read needs only an execution dependency, so the source has a stage but no access
		const vk::ImageMemoryBarrier2 toStorage = MakeImageBarrier(target.Image,
			{ .Layout = vk::ImageLayout::eUndefined, .Access = vk::AccessFlagBits2::eNone, .Stage = vk::PipelineStageFlagBits2::eFragmentShader },
			kComputeStorageWrite, kColorRange);
		frame.CommandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toStorage });

		{
			GPUDebugScope computePass(m_RenderDevice, frame.CommandBuffer, "Test Compute Pass");
			frame.CommandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);

			GPU::ComputePushConstants pushConstants{
				.Global = globalPCs,
				.TextureIndex = target.Index,
				.SamplerIndex = static_cast<uint32_t>(SamplerType::LinearClamp)
			};
			ApplyPushConstants(*frame.CommandBuffer, m_PipelineStore.GetGlobalLayout(), pushConstants);

			// dispatch takes a group count, rounded up so the groups cover the whole target
			const LocalGroupSize& groupSize = compEntryPoint->LocalSize;
			frame.CommandBuffer.dispatch((target.Extent.x + groupSize.X - 1) / groupSize.X, (target.Extent.y + groupSize.Y - 1) / groupSize.Y, 1);
		}

		// The scene pass samples it next
		const vk::ImageMemoryBarrier2 toSampled = MakeImageBarrier(target.Image, kComputeStorageWrite, FragmentSampled(target), kColorRange);
		frame.CommandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toSampled });
	}

	void Renderer::EnsureCapacity(GPUBuffer& buffer, size_t size, vk::BufferUsageFlags usage, std::string_view name)
	{
		if (buffer.GetSize() >= size)
			return;

		buffer = m_GPUAllocator.CreateBuffer({
			.Size = std::max<size_t>(size, buffer.GetSize() * 2),   // Doubling, so a growing UI doesn't reallocate every frame
			.Usage = usage,
			.Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.DebugName = std::string(name) 
		});
	}
}