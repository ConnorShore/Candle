#include "cdlpch.h"
#include "PipelineStore.h"
#include "ShaderCreationInfo.h"

#include "Candle/Render/Vulkan/RenderDevice.h"
#include "Candle/Render/Vulkan/BindlessTable.h"

#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"

namespace Candle {

	namespace {

		constexpr std::array<vk::DynamicState, 4> kDynamicStates = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor,
			vk::DynamicState::eDepthTestEnable,
			vk::DynamicState::eLineWidth
		};
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

		bool ContainsEntryPoint(const std::vector<ShaderEntryPoint>& entryPoints, const std::string& entryPointName)
		{
			if (entryPointName.empty())
				return true;
			return std::any_of(entryPoints.begin(), entryPoints.end(),
				[&](const ShaderEntryPoint& entryPoint) { return entryPoint.Name == entryPointName; });
		}

	}

	PipelineStore::PipelineStore(RenderDevice& renderDevice, BindlessTable& bindlessTable)
		: m_RenderDevice(renderDevice)
		, m_BindlessTable(bindlessTable)
		, m_GlobalLayout(std::move(CreateGlobalLayout()))
	{
	}

	GPUPipelineHandle PipelineStore::CreatePipeline(PipelineSpecification spec)
	{
		PipelineEntry entry;
		std::visit(Overloaded{
			[&](const GraphicsPipelineSpecification& graphicsSpec) { entry = CreateGraphicsPipelineEntry(graphicsSpec); },
			[&](const ComputePipelineSpecification& computeSpec) { entry = CreateComputePipelineEntry(computeSpec); }
			}, std::move(spec));

		m_Pipelines.push_back(std::move(entry));
		return GPUPipelineHandle{ static_cast<uint32_t>(m_Pipelines.size() - 1), 0 };
	}

	vk::Pipeline PipelineStore::TryGetPipeline(GPUPipelineHandle handle)
	{
		if (handle.Index >= m_Pipelines.size())
			return nullptr;
		if (m_Pipelines[handle.Index].Pipeline == nullptr)
			return nullptr;

		return *m_Pipelines[handle.Index].Pipeline;
	}

	vk::raii::PipelineLayout PipelineStore::CreateGlobalLayout()
	{
		vk::PushConstantRange pushConstantRange{
			.stageFlags = kPushConstantStages,
			.offset = 0,
			.size = kPushConstantsSize
		};

		vk::DescriptorSetLayout layouts[] = { m_BindlessTable.GetLayout() };
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
			.setLayoutCount = 1,
			.pSetLayouts = layouts,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pushConstantRange
		};

		auto layout = vk::raii::PipelineLayout{ m_RenderDevice.GetDevice(), pipelineLayoutInfo };
		m_RenderDevice.SetDebugName(*layout,  "Global Pipeline Layout");

		return std::move(layout);
	}

	PipelineStore::PipelineEntry PipelineStore::CreateGraphicsPipelineEntry(const GraphicsPipelineSpecification& spec)
	{
		bool containsDepthAttachment = spec.DepthAttachmentFormat != vk::Format::eUndefined;

#pragma region Shader Module
		// Create shader modules and pipeline shader stage create info for each entry point
		vk::raii::ShaderModule shaderModule = spec.Shader->CreateModule(m_RenderDevice);
		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = std::move(CreateShaderStages(shaderModule, spec.Shader->GetEntryPoints()));

		// Validate the entry point names exist in the shader module and that the push-constant blocks are valid
		// TOOD: Handle cases like this more gracefully, for now we just crash the engine if the entry point doesn't exist
		if (!ContainsEntryPoint(spec.Shader->GetEntryPoints(), spec.VertexEntryPoint))
			CDL_CORE_FATAL(LogChannel::Render, "Vertex entry point '" + spec.VertexEntryPoint + "' not found in shader module");
		if (!ContainsEntryPoint(spec.Shader->GetEntryPoints(), spec.FragmentEntryPoint))
			CDL_CORE_FATAL(LogChannel::Render, "Fragment entry point '" + spec.FragmentEntryPoint + "' not found in shader module");
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
		// Disabling right now, will revisit later once I get to the multisampling chapter
		vk::PipelineMultisampleStateCreateInfo multisampling{
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
			.sampleShadingEnable = vk::False
		};
#pragma endregion

#pragma region Depth / Stencil State
		vk::PipelineDepthStencilStateCreateInfo depthStencil{
			.depthTestEnable = vk::True,	// Overridden with dynamic state
			.depthWriteEnable = vk::True,	// Overridden with dynamic state
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

#pragma region Validate Push Constants
		std::vector<ShaderEntryPoint> entryPointsToValidate;
		// Only validate the entry points that are actually used for this pipeline, rather than all the entry points in the shader
		auto addEntryPointIfExists = [&](const std::string& entryPointName) {
			if (entryPointName.empty())
				return;

			auto it = std::find_if(spec.Shader->GetEntryPoints().begin(), spec.Shader->GetEntryPoints().end(),
				[&](const ShaderEntryPoint& entryPoint) { return entryPoint.Name == entryPointName; });
			if (it != spec.Shader->GetEntryPoints().end())
				entryPointsToValidate.push_back(*it);
		};
		addEntryPointIfExists(spec.VertexEntryPoint);
		addEntryPointIfExists(spec.FragmentEntryPoint);

		ValidatePushConstants(entryPointsToValidate);
#pragma endregion

#pragma region Graphics Pipeline Creation
		vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
			.colorAttachmentCount = static_cast<uint32_t>(spec.ColorAttachmentFormats.size()),
			.pColorAttachmentFormats = spec.ColorAttachmentFormats.data(),
			.depthAttachmentFormat = spec.DepthAttachmentFormat
		};

		// Create the graphics pipeline
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};	// Empty by design: shaders pull vertices through a device address in the push constants
		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain{
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
				.layout = *m_GlobalLayout,
				.renderPass = nullptr, // Nullptr because we are using dynamic rendering
				.subpass = 0
			}, pipelineRenderingCreateInfo
		};
		auto pipeline = vk::raii::Pipeline(m_RenderDevice.GetDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
		if (!spec.DebugName.empty())
			m_RenderDevice.SetDebugName(*pipeline, spec.DebugName);
#pragma endregion

		return PipelineEntry{
			.Pipeline{ std::move(pipeline) },
			.Spec{ spec }
		};
	}

	PipelineStore::PipelineEntry PipelineStore::CreateComputePipelineEntry(const ComputePipelineSpecification& spec)
	{
		throw std::runtime_error("Compute pipeline creation not implemented yet");
		return {};
	}

}