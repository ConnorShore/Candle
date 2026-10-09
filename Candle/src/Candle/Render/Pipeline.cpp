#include "cdlpch.h"
#include "Pipeline.h"
#include "RenderDevice.h"

#include <array>

namespace Candle {

	namespace {

		constexpr std::array<vk::DynamicState, 2> kDynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		constexpr vk::FrontFace kDefaultFrontFace = vk::FrontFace::eCounterClockwise;

		vk::ShaderStageFlagBits ShaderStageToVulkanShaderStage(ShaderStage stage)
		{
			switch (stage)
			{
			case ShaderStage::Vertex: return vk::ShaderStageFlagBits::eVertex;
			case ShaderStage::Fragment: return vk::ShaderStageFlagBits::eFragment;
			case ShaderStage::Compute: return vk::ShaderStageFlagBits::eCompute;
			default:
				CDL_CORE_ASSERT(false, "Unknown shader stage");
				return vk::ShaderStageFlagBits::eVertex; // Default to vertex shader stage
			}
		}

		std::vector<vk::PipelineShaderStageCreateInfo> CreateShaderStages(vk::raii::ShaderModule& shaderModule, const std::vector<ShaderEntryPoint>& entryPoints)
		{
			std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
			shaderStages.reserve(entryPoints.size());

			for (const auto& entryPoint : entryPoints)
			{
				vk::PipelineShaderStageCreateInfo shaderStageInfo{
					.stage = ShaderStageToVulkanShaderStage(entryPoint.Stage),
					.module = shaderModule,
					.pName = entryPoint.Name.c_str()
				};
				shaderStages.push_back(shaderStageInfo);
			}

			return shaderStages;
		}

		vk::PipelineColorBlendAttachmentState CreateColorBlendAttachmentState(BlendMode blendMode)
		{
			vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
			colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
			switch (blendMode)
			{
			case BlendMode::None:
				colorBlendAttachment.blendEnable = vk::False;
				break;
			case BlendMode::SrcAlpha:
				colorBlendAttachment.blendEnable = vk::True;
				colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
				colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
				colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
				colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
				colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
				colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
				break;
			case BlendMode::OneMinusSrcAlpha:
				colorBlendAttachment.blendEnable = vk::True;
				colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
				colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eSrcAlpha;
				colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
				colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
				colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
				colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
				break;
			default:
				CDL_CORE_ASSERT(false, "Unknown blend mode");
			}
			return colorBlendAttachment;
		}

	}
	

	Pipeline::Pipeline(PipelineSpecification spec, RenderDevice& renderDevice, vk::Format format)
		: m_Specification(std::move(spec))
	{
		CreatePipeline(format, renderDevice);
	}

	void Pipeline::CreatePipeline(vk::Format format, RenderDevice& renderDevice)
	{
#pragma region Shader Module
		// Create shader modules and pipeline shader stage create info for each entry point
		vk::raii::ShaderModule shaderModule = m_Specification.Shader->CreateModule(renderDevice);
		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = std::move(CreateShaderStages(shaderModule, m_Specification.Shader->GetEntryPoints()));
#pragma endregion

#pragma region Input Assembly State
		// Input assembly state describes how primitives are assembled from vertices
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = static_cast<vk::PrimitiveTopology>(m_Specification.Topology) };
#pragma endregion

#pragma region Dynamic State
		// Set dynamic states that can be changed without recreating the pipeline
		vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(kDynamicStates.size()), .pDynamicStates = kDynamicStates.data() };
		vk::PipelineViewportStateCreateInfo viewportState{ .viewportCount = 1, .scissorCount = 1 };
#pragma endregion

#pragma region Rasterization State
		// Rasterization state describes how primitives are rasterized into fragments
		// Using defaults for now but may make some of these configurable in the future
		vk::PipelineRasterizationStateCreateInfo rasterizer{ .depthClampEnable = vk::False,
													.rasterizerDiscardEnable = vk::False,
													.polygonMode = vk::PolygonMode::eFill,
													.cullMode = m_Specification.CullBackFaces ? vk::CullModeFlagBits::eBack : vk::CullModeFlagBits::eNone,
													.frontFace = kDefaultFrontFace,
													.depthBiasEnable = vk::False,
													.lineWidth = 1.0f };
#pragma endregion

#pragma region Multisample State
		// Disablign right now, will revisit later once I get to the multisampling chapter
		vk::PipelineMultisampleStateCreateInfo multisampling{ .rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False };
#pragma endregion

#pragma region Depth / Stencil State
		// Disabling depth and stencil testing for now, will revisit later once I get to the depth and stencil chapter
		vk::PipelineDepthStencilStateCreateInfo depthStencil{ .depthTestEnable = vk::False, .depthWriteEnable = vk::False, .depthCompareOp = vk::CompareOp::eLessOrEqual, .depthBoundsTestEnable = vk::False, .stencilTestEnable = vk::False };
#pragma endregion

#pragma region Color Blend State
		// Color blend state describes how the output from the fragment shader is blended with the existing color in the framebuffer
		vk::PipelineColorBlendAttachmentState colorBlendAttachment = CreateColorBlendAttachmentState(m_Specification.Blend);
		vk::PipelineColorBlendStateCreateInfo colorBlending{.logicOpEnable = vk::False, 
														.logicOp = vk::LogicOp::eCopy,
														.attachmentCount = 1,
														.pAttachments = &colorBlendAttachment };
#pragma endregion

#pragma region Pipeline Layout
		// Create pipeline layout (descriptor set layouts, push constants)
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{ .setLayoutCount = 0, .pushConstantRangeCount = 0 };
		m_Layout = vk::raii::PipelineLayout{ renderDevice.GetDevice(), pipelineLayoutInfo };
#pragma endregion

#pragma region Graphics Pipeline Creation
		vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
			.colorAttachmentCount = static_cast<uint32_t>(m_Specification.ColorAttachmentFormats.size()),
			.pColorAttachmentFormats = m_Specification.ColorAttachmentFormats.data(),
			.depthAttachmentFormat = m_Specification.DepthAttachmentFormat};

		// Create the graphics pipeline
		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
													{
													 .stageCount = static_cast<uint32_t>(shaderStages.size()),
													 .pStages = shaderStages.data(),
													 .pVertexInputState = nullptr, // No vertex input for now, will revisit later once I get to the vertex input chapter
													 .pInputAssemblyState = &inputAssembly,
													 .pViewportState = &viewportState,
													 .pRasterizationState = &rasterizer,
													 .pMultisampleState = &multisampling,
													 .pDepthStencilState = &depthStencil,
													 .pColorBlendState = &colorBlending,
													 .pDynamicState = &dynamicState,
													 .layout = *m_Layout,
													 .renderPass = nullptr, // Nullptr because we are using dynamic rendering
													 .subpass = 0 
													}, pipelineRenderingCreateInfo };
		m_Pipeline = vk::raii::Pipeline(renderDevice.GetDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
	}

}