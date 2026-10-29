#include "cdlpch.h"

#include "Renderer.h"
#include "RenderDevice.h"
#include "PipelineSpecification.h"
#include "FramePacket.h"
#include "FrameContext.h"
#include "SwapChainImage.h"
#include "GeometryPool.h"
#include "TexturePool.h"
#include "GPUAllocator.h"

#include "Candle/Render/Pipelines/TrianglePipeline.h"
#include "Candle/Render/Pipelines/TriangleInterop.h"

#include <cmath>
#include <span>

namespace Candle {

	namespace {
		// A float time steps ~0.24 ms at an hour; wrapping keeps shader animation smooth for one jump per period
		constexpr double kShaderTimeWrapSeconds = 3600.0;

		vk::Format FindDepthFormat(RenderDevice& device)
		{
			// Depth-only, so the depth-aspect view and barrier stay valid without separateDepthStencilLayouts. The spec requires
			// D16 and one of the first two as attachments, so the throw below can't be reached on a conformant device.
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
			throw std::runtime_error("No suitable depth format found.");
		}
	}

	Renderer::Renderer(RenderDevice& renderDevice, GeometryPool& geometryPool, TexturePool& texturePool, BindlessTable& bindlessTable, GPUAllocator& allocator, vk::Format targetFormat, vk::Extent2D targetExtent)
		: m_RenderDevice(renderDevice)
		, m_GeometryPool(geometryPool)
		, m_TexturePool(texturePool)
		, m_BindlessTable(bindlessTable)
		, m_TargetFormat(targetFormat)
		, m_DepthFormat(FindDepthFormat(renderDevice))
		, m_DepthImage({ .m_Extent = { targetExtent.width, targetExtent.height }, // Sized to the colour target; the render graph will eventually own this per target
			.m_Format = m_DepthFormat,
			.m_Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment, 
			.m_Aspect = vk::ImageAspectFlagBits::eDepth }, allocator)
	{
		CreatePipeline(TrianglePipeline{});
	}

	void Renderer::Render(const FramePacket& packet, FrameContext& frame, SwapChainImage& target)
	{
		CDL_PROFILE_SCOPE("Renderer::Render");

		// Write the transform data into the frame's TransformData buffer, if any
		const std::span<const glm::mat4> transforms = packet.TransformData;
		if (!transforms.empty())
			frame.TransformData.Write(std::as_bytes(transforms));

		// Safe to overwrite: RenderManager waited on this slot's RetireValue, and vkQueueSubmit makes host writes visible to the GPU
		const GPU::FrameData frameData{ 
			.ViewProjection = packet.ProjectionMatrix * packet.ViewMatrix,
			.TransformData = frame.TransformData.GetDeviceAddress(),
			.TransformCount = static_cast<uint32_t>(transforms.size()),
			.Time = static_cast<float>(std::fmod(packet.Time, kShaderTimeWrapSeconds))
		};
		frame.FrameData.Write(std::as_bytes(std::span(&frameData, 1)));

		const GPU::GlobalPushConstants global{ 
			.Frame = frame.FrameData.GetDeviceAddress() 
		};

		// For now, just record a command buffer that draws a triangle to the target image using the first pipeline in m_Pipelines
		// In the future, this will be more complex and involve a render graph and multiple pipelines
		frame.CommandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		// Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
		target.Transition({
			.Layout = vk::ImageLayout::eColorAttachmentOptimal,
			.Access = vk::AccessFlagBits2::eColorAttachmentWrite,
			.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput
			}, frame.CommandBuffer);

		// Transition the depth image to DEPTH_ATTACHMENT_OPTIMAL
		// This will be done in the render graph in the future, but for now we do it here
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
			.imageView = target.GetImageView(),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor };

		vk::RenderingAttachmentInfo depthAttachmentInfo = {
			.imageView = m_DepthImage.GetImageView(),
			.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = clearDepth 
		};

		vk::Extent2D swapChainExtent = target.GetDesc().Extent;

		// Setup the rendering info for the render pass begin info. This specifies the render area, the number of layers, and the color attachment info.
		vk::RenderingInfo renderingInfo = {
			.renderArea = {
				.offset = {0, 0}, 
				.extent = swapChainExtent },	
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo,
			.pDepthAttachment = &depthAttachmentInfo
		};

		frame.CommandBuffer.beginRendering(renderingInfo);

		// Only one pipeline for now, but eventually this will be chosen based on the packet and the render graph
		frame.CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Pipelines[0].GetPipeline());

		frame.CommandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(swapChainExtent.height), static_cast<float>(swapChainExtent.width), -static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
		frame.CommandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

		// Bind the bindless descriptor set to the pipeline layout
		vk::PipelineLayout pipelineLayout = m_Pipelines[0].GetLayout();
		m_BindlessTable.BindSet(*frame.CommandBuffer, vk::PipelineBindPoint::eGraphics, pipelineLayout);

		// Draw each item
		for (const DrawItem& item : packet.DrawItems)
		{
			// The GPU doesn't bounds-check a read through a device address, so catch a bad index here
			CDL_CORE_ASSERT(item.TransformIndex < transforms.size(), "DrawItem's TransformIndex is past the packet's TransformData");

			MeshDrawInfo drawInfo = m_GeometryPool.GetMeshInfo(item.Mesh);
			TextureInfo textureInfo = m_TexturePool.GetTextureInfo(item.Material);

			GPU::MeshDrawPushConstants pushConstants{
				.Global = global,
				.Vertices = drawInfo.VerticesAddr,
				.TextureIndex = textureInfo.Index,
				.SamplerIndex = static_cast<uint32_t>(item.Sampler),
				.TransformIndex = item.TransformIndex
			};
			m_Pipelines[0].PushConstants(*frame.CommandBuffer, pushConstants);
			frame.CommandBuffer.bindIndexBuffer(drawInfo.Indices, 0, vk::IndexType::eUint32);
			frame.CommandBuffer.drawIndexed(drawInfo.IndexCount, 1, 0, 0, 0);
		}

		// End the render pass.
		frame.CommandBuffer.endRendering();

		target.Transition({
			.Layout = vk::ImageLayout::ePresentSrcKHR,
			.Access = vk::AccessFlagBits2::eNone
			}, frame.CommandBuffer);

		// End recording commands into the command buffer. After this, the command buffer can be submitted to the graphics queue for execution.
		frame.CommandBuffer.end();
	}

	void Renderer::RecreateTargets(vk::Extent2D targetExtent, GPUAllocator& allocator)
	{
		m_DepthImage.Recreate({ .m_Extent = { targetExtent.width, targetExtent.height },
			.m_Format = m_DepthFormat,
			.m_Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
			.m_Aspect = vk::ImageAspectFlagBits::eDepth }, allocator);
	}

	void Renderer::CreatePipeline(const RenderPipeline& pipeline)
	{
		m_Pipelines.emplace_back(pipeline.CreateSpecification(m_RenderDevice, m_TargetFormat, m_DepthFormat), m_RenderDevice, m_BindlessTable, m_TargetFormat);
	}

}