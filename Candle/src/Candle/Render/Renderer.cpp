#include "cdlpch.h"

#include "Renderer.h"
#include "RenderDevice.h"
#include "PipelineSpecification.h"
#include "ShaderLoader.h"
#include "FramePacket.h"
#include "FrameContext.h"
#include "Image.h"
#include "GeometryPool.h"

namespace Candle {

	// Temporary push constant struct for the triangle shader (will eventually live with each pipeline)
	// TODO: Maybe move to a TriangleRenderPass class that owns the pipeline and the push constants to mimic future structure
	struct MeshDrawPushConstants
	{
		vk::DeviceAddress VertexBufferAddress;
	};
	CDL_STATIC_ASSERT(sizeof(MeshDrawPushConstants) == 8, "Push constants out of sync with the shader");

	Renderer::Renderer(RenderDevice& renderDevice, GeometryPool& geometryPool, vk::Format targetFormat)
		: m_RenderDevice(renderDevice), m_GeometryPool(geometryPool), m_TargetFormat(targetFormat)
	{
		// Create triangle pipeline here for now, will eventually be created on demand when a pipeline is needed
		SharedPtr<Shader> shader = ShaderLoader::LoadShader(
			"BasicTriangle", 
			"C:/Development/Projects/Candle/Candle/res/shaders/bin/Triangle.spv",
			m_RenderDevice
		);
		PipelineSpecification spec{
			.Shader = shader,
			.ColorAttachmentFormats = {m_TargetFormat}
			// Rest are defaults for now
		};

		CreatePipeline(spec);
	}

	void Renderer::Render(const FramePacket& packet, FrameContext& frame, Image& target)
	{
		// For now, just record a command buffer that draws a triangle to the target image using the first pipeline in m_Pipelines
		// In the future, this will be more complex and involve a render graph and multiple pipelines
		frame.CommandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		// Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
		target.Transition({
			.Layout = vk::ImageLayout::eColorAttachmentOptimal,
			.AccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.StageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.AspectMask = vk::ImageAspectFlagBits::eColor
			}, frame.CommandBuffer);

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::RenderingAttachmentInfo colorAttachmentInfo = {
			.imageView = target.GetImageView(),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor };

		vk::Extent2D swapChainExtent = target.GetDesc().Extent;

		// Setup the rendering info for the render pass begin info. This specifies the render area, the number of layers, and the color attachment info.
		vk::RenderingInfo renderingInfo = {
			.renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo};

		frame.CommandBuffer.beginRendering(renderingInfo);

		// Only one pipeline for now, but eventually this will be chosen based on the packet and the render graph
		frame.CommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Pipelines[0].GetPipeline());

		frame.CommandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(swapChainExtent.height), static_cast<float>(swapChainExtent.width), -static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
		frame.CommandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

		// Draw a triangle with 3 vertices, 1 instance, starting at vertex 0 and instance 0
		// (Vertex data is generated in the vertex shader, so no vertex buffer is bound)
		//frame.CommandBuffer.draw(3, 1, 0, 0);

		// Push constants for the triangle shader (will eventually be set per mesh)
		// TODO: Maybe pass the triangle mesh to the frame packet
		MeshDrawInfo drawInfo = m_GeometryPool.GetMeshInfo(packet.Meshes[0]);
		MeshDrawPushConstants pushConstants{.VertexBufferAddress = drawInfo.VerticesAddr };
		frame.CommandBuffer.pushConstants<MeshDrawPushConstants>(m_Pipelines[0].GetLayout(), vk::ShaderStageFlagBits::eVertex, 0, pushConstants);
		frame.CommandBuffer.bindIndexBuffer(drawInfo.Indices, 0, vk::IndexType::eUint32);

		frame.CommandBuffer.drawIndexed(drawInfo.IndexCount, 1, 0, 0, 0);

		// End the render pass.
		frame.CommandBuffer.endRendering();

		target.Transition({
			.Layout = vk::ImageLayout::ePresentSrcKHR,
			.AccessMask = vk::AccessFlagBits2::eNone,
			.AspectMask = vk::ImageAspectFlagBits::eColor,
			}, frame.CommandBuffer);

		// End recording commands into the command buffer. After this, the command buffer can be submitted to the graphics queue for execution.
		frame.CommandBuffer.end();
	}

	void Renderer::CreatePipeline(PipelineSpecification& spec)
	{
		m_Pipelines.emplace_back(spec, m_RenderDevice, m_TargetFormat);
	}

}