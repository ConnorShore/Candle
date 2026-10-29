#include "cdlpch.h"
#include "Pipeline.h"
#include "RenderDevice.h"
#include "BindlessTable.h"

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
			case BlendMode::Opaque:
				colorBlendAttachment.blendEnable = vk::False;
				break;
			case BlendMode::AlphaBlend:
				colorBlendAttachment.blendEnable = vk::True;
				colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
				colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
				colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
				colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
				colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
				colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
				break;
			case BlendMode::PremultipliedAlpha:
				colorBlendAttachment.blendEnable = vk::True;
				colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne;
				colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
				colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
				colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
				colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
				colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
				break;
			case BlendMode::Additive:
				colorBlendAttachment.blendEnable = vk::True;
				colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne;
				colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOne;
				colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
				colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
				colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOne;
				colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
				break;
			default:
				CDL_CORE_ASSERT(false, "Unknown blend mode");
			}
			return colorBlendAttachment;
		}



		// The shader half of the push-constant contract; Pipeline::PushConstants checks the C++ half
		void ValidatePushConstants(const std::vector<ShaderEntryPoint>& entryPoints)
		{
			for (const ShaderEntryPoint& entryPoint : entryPoints)
			{
				const PushConstantBlock& block = entryPoint.PushConstants;
				if (block.Size == 0)
					continue;

				CDL_CORE_ASSERT(block.Size <= kPushConstantsSize, "Push-constant block exceeds the engine-wide range");
				CDL_CORE_ASSERT(!block.Members.empty() && block.Members[0].Name == "Global" && block.Members[0].Offset == 0
					&& block.Members[0].Size == sizeof(GPU::GlobalPushConstants),
					"A push-constant block must start with 'GlobalPushConstants Global' (see ShaderInterop/GlobalPushConstants.h)");
			}
		}

	}
	

	Pipeline::Pipeline(const PipelineSpecification& spec, RenderDevice& renderDevice, BindlessTable& bindlessTable)
	{
		CreatePipeline(spec, renderDevice, bindlessTable);
	}

	void Pipeline::CreatePipeline(const PipelineSpecification& spec, RenderDevice& renderDevice, BindlessTable& bindlessTable)
	{
		bool containsDepthAttachment = spec.DepthAttachmentFormat != vk::Format::eUndefined;

#pragma region Shader Module
		// Create shader modules and pipeline shader stage create info for each entry point
		vk::raii::ShaderModule shaderModule = spec.Shader->CreateModule(renderDevice);
		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = std::move(CreateShaderStages(shaderModule, spec.Shader->GetEntryPoints()));
#pragma endregion

#pragma region Input Assembly State
		// Input assembly state describes how primitives are assembled from vertices
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{ .topology = static_cast<vk::PrimitiveTopology>(spec.Topology) };
#pragma endregion

#pragma region Dynamic State
		// Set dynamic states that can be changed without recreating the pipeline
		vk::PipelineDynamicStateCreateInfo dynamicState{ 
			.dynamicStateCount = static_cast<uint32_t>(kDynamicStates.size()), 
			.pDynamicStates = kDynamicStates.data() 
		};
		vk::PipelineViewportStateCreateInfo viewportState{ 
			.viewportCount = 1, 
			.scissorCount = 1 
		};
#pragma endregion

#pragma region Rasterization State
		// Rasterization state describes how primitives are rasterized into fragments
		// Using defaults for now but may make some of these configurable in the future
		vk::PipelineRasterizationStateCreateInfo rasterizer{ 
			.depthClampEnable = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = spec.CullBackFaces ? vk::CullModeFlagBits::eBack : vk::CullModeFlagBits::eNone,
			.frontFace = kDefaultFrontFace,
			.depthBiasEnable = vk::False,
			.lineWidth = 1.0f
		};
#pragma endregion

#pragma region Multisample State
		// Disablign right now, will revisit later once I get to the multisampling chapter
		vk::PipelineMultisampleStateCreateInfo multisampling{ 
			.rasterizationSamples = vk::SampleCountFlagBits::e1, 
			.sampleShadingEnable = vk::False 
		};
#pragma endregion

#pragma region Depth / Stencil State
		vk::PipelineDepthStencilStateCreateInfo depthStencil{ 
			.depthTestEnable = containsDepthAttachment ? vk::True : vk::False, 
			.depthWriteEnable = containsDepthAttachment ? vk::True : vk::False,
			.depthCompareOp = vk::CompareOp::eLessOrEqual, 
			.depthBoundsTestEnable = vk::False, 
			.stencilTestEnable = vk::False 
		};
#pragma endregion

#pragma region Color Blend State
		// Color blend state describes how the output from the fragment shader is blended with the existing color in the framebuffer
		// One blend state per color attachment, as the count must match the rendering info's
		std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments(spec.ColorAttachmentFormats.size(), CreateColorBlendAttachmentState(spec.Blend));
		vk::PipelineColorBlendStateCreateInfo colorBlending{
			.logicOpEnable = vk::False,
			.logicOp = vk::LogicOp::eCopy,
			.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size()),
			.pAttachments = colorBlendAttachments.data()
		};
#pragma endregion

#pragma region Pipeline Layout
		// Create pipeline layout (descriptor set layouts, push constants)
		// Every layout declares the same range whatever its shader uses, so all are compatible for push constants
		ValidatePushConstants(spec.Shader->GetEntryPoints());
		vk::PushConstantRange pushConstantRange{ 
			.stageFlags = kPushConstantStages, 
			.offset = 0, 
			.size = kPushConstantsSize 
		};

		vk::DescriptorSetLayout layouts[] = { bindlessTable.GetLayout() };
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
			.setLayoutCount = 1,
			.pSetLayouts = layouts,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pushConstantRange
		};
		m_Layout = vk::raii::PipelineLayout{ renderDevice.GetDevice(), pipelineLayoutInfo };
		if (!spec.DebugName.empty())
			renderDevice.SetDebugName(*m_Layout, spec.DebugName + " Layout");
#pragma endregion

#pragma region Graphics Pipeline Creation
		vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
			.colorAttachmentCount = static_cast<uint32_t>(spec.ColorAttachmentFormats.size()),
			.pColorAttachmentFormats = spec.ColorAttachmentFormats.data(),
			.depthAttachmentFormat = spec.DepthAttachmentFormat
		};

		// Create the graphics pipeline
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};	// Empty by design: shaders pull vertices through a device address in the push constants
		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
													{
													 .stageCount = static_cast<uint32_t>(shaderStages.size()),
													 .pStages = shaderStages.data(),
													 .pVertexInputState = &vertexInputInfo,
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
		if (!spec.DebugName.empty())
			renderDevice.SetDebugName(*m_Pipeline, spec.DebugName);
	}

}