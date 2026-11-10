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

		vk::PipelineShaderStageCreateInfo CreateShaderStage(vk::raii::ShaderModule& shaderModule, const std::vector<ShaderEntryPoint>& entryPoints, const std::string& targetEntryPoint)
		{
			auto entryPoint = std::find_if(entryPoints.begin(), entryPoints.end(),
				[&](const ShaderEntryPoint& ep) { return ep.Name == targetEntryPoint; });

			CDL_CORE_ASSERT(entryPoint != entryPoints.end(), "Entry point '{}' not found in shader module", targetEntryPoint);

			return vk::PipelineShaderStageCreateInfo{
				.stage = ShaderStageToVulkanShaderStage(entryPoint->Stage),
				.module = *shaderModule,
				.pName = entryPoint->Name.c_str()
			};
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

		std::expected<void, std::string> ValidateGraphicsEntryPoints(const std::vector<ShaderEntryPoint>& entryPoints, const GraphicsPipelineSpecification& spec)
		{
			if (spec.VertexEntryPoint.empty())
				return std::unexpected("At least one VertexEntryPoint must be specified for a graphics pipeline");
			if (!ContainsEntryPoint(entryPoints, spec.VertexEntryPoint))
				return std::unexpected("Vertex entry point '" + spec.VertexEntryPoint + "' not found in shader module");
			if (!ContainsEntryPoint(entryPoints, spec.FragmentEntryPoint))
				return std::unexpected("Fragment entry point '" + spec.FragmentEntryPoint + "' not found in shader module");

			// Verify vertex entry point is for a vertex entry and fragment entry point is for fragment entry
			auto validateEntryPoint = [&](const std::string& entryPointName, ShaderStage expectedStage) -> std::expected<void, std::string> {
				if (entryPointName.empty())
					return {};

				auto entryPointIt = std::find_if(entryPoints.begin(), entryPoints.end(),
					[&](const ShaderEntryPoint& ep) { return ep.Name == entryPointName; });
				if (entryPointIt == entryPoints.end())
					return std::unexpected("Entry point '" + entryPointName + "' not found in shader module");
				if (entryPointIt->Stage != expectedStage)
					return std::unexpected("Entry point '" + entryPointName + "' is not a " + std::string(expectedStage == ShaderStage::Vertex ? "vertex" : "fragment") + " shader entry point");
				return {};
				};

			if (auto result = validateEntryPoint(spec.VertexEntryPoint, ShaderStage::Vertex); !result)
				return std::unexpected(result.error());
			if (auto result = validateEntryPoint(spec.FragmentEntryPoint, ShaderStage::Fragment); !result)
				return std::unexpected(result.error());

			return {};
		}

		std::expected<void, std::string> ValidateComputeEntryPoints(const std::vector<ShaderEntryPoint>& entryPoints, const ComputePipelineSpecification& spec)
		{
			if (spec.EntryPoint.empty())
				return std::unexpected("A ComputeEntryPoint must be specified for a compute pipeline");
			if (!ContainsEntryPoint(entryPoints, spec.EntryPoint))
				return std::unexpected("Compute entry point '" + spec.EntryPoint + "' not found in shader module");

			// Verify compute entry point is for a compute entry
			auto entryPointIt = std::find_if(entryPoints.begin(), entryPoints.end(),
				[&](const ShaderEntryPoint& ep) { return ep.Name == spec.EntryPoint; });
			if (entryPointIt == entryPoints.end())
				return std::unexpected("Compute entry point '" + spec.EntryPoint + "' not found in shader module");
			if (entryPointIt->Stage != ShaderStage::Compute)
				return std::unexpected("Entry point '" + spec.EntryPoint + "' is not a compute shader entry point");

			return {};
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
		std::expected<PipelineEntry, std::string> entry;
		std::visit(Overloaded{
			[&](const GraphicsPipelineSpecification& graphicsSpec) { entry = CreateGraphicsPipelineEntry(graphicsSpec); },
			[&](const ComputePipelineSpecification& computeSpec) { entry = CreateComputePipelineEntry(computeSpec); }
			}, std::move(spec));

		if (!entry)
		{
			CDL_CORE_ERROR(LogChannel::Render, "Failed to create pipeline: {}", entry.error());
			return {};	// Return invalid handle
		}

		m_Pipelines.push_back(std::move(entry.value()));
		return GPUPipelineHandle{ static_cast<uint32_t>(m_Pipelines.size() - 1), 0 };
	}

	vk::Pipeline PipelineStore::TryGetPipeline(GPUPipelineHandle handle)
	{
		
		if (auto valid = ValidateHandle(handle); !valid.has_value())
			return nullptr;

		return m_Pipelines[handle.Index].Pipeline;
	}

	std::expected<ShaderEntryPoint, std::string> PipelineStore::GetEntryPoint(GPUPipelineHandle handle, ShaderStage stage) const
	{
		if (auto valid = ValidateHandle(handle); !valid.has_value())
			return std::unexpected(valid.error());

		size_t stageIndex = static_cast<size_t>(stage);
		return std::visit(Overloaded{
			[&](const GraphicsPipelineSpecification& spec) -> ShaderEntryPoint { return spec.Shader->GetEntryPoints()[stageIndex]; },
			[&](const ComputePipelineSpecification& spec) -> ShaderEntryPoint { return spec.Shader->GetEntryPoints()[stageIndex]; }
			}, m_Pipelines[handle.Index].Spec);
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

		return layout;
	}

	std::expected<PipelineStore::PipelineEntry, std::string> PipelineStore::CreateGraphicsPipelineEntry(const GraphicsPipelineSpecification& spec)
	{
		bool containsDepthAttachment = spec.DepthAttachmentFormat != vk::Format::eUndefined;

		if (!spec.Shader)
			return std::unexpected("Shader is null");

		auto entryPoints = spec.Shader->GetEntryPoints();

#pragma region Validate Push Constants
		// TODO: Validate push constants only over the entry points used by this pipeline, not all entry points in the shader
		ValidatePushConstants(entryPoints);
#pragma endregion

#pragma region Shader Module
		// Validate that the specified entry points exist in the shader module
		std::expected<void, std::string> validationResult = ValidateGraphicsEntryPoints(entryPoints, spec);
		if (!validationResult)
			return std::unexpected(validationResult.error());

		// Create shader modules and pipeline shader stage create info for each entry point
		vk::raii::ShaderModule shaderModule = spec.Shader->CreateModule(m_RenderDevice);

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
		shaderStages.reserve(2);
		if (!spec.VertexEntryPoint.empty())
			shaderStages.push_back(CreateShaderStage(shaderModule, entryPoints, spec.VertexEntryPoint));
		if (!spec.FragmentEntryPoint.empty())
			shaderStages.push_back(CreateShaderStage(shaderModule, entryPoints, spec.FragmentEntryPoint));
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
			.Spec{ spec },
		};
	}

	std::expected<PipelineStore::PipelineEntry, std::string> PipelineStore::CreateComputePipelineEntry(const ComputePipelineSpecification& spec)
	{
		if (!spec.Shader)
			return std::unexpected("Shader is null");
		if (spec.EntryPoint.empty())
			return std::unexpected("Compute entry point must be specified");

		auto entryPoints = spec.Shader->GetEntryPoints();

#pragma region Validate Push Constants
		// TODO: Validate push constants only over the entry points used by this pipeline, not all entry points in the shader
		ValidatePushConstants(entryPoints);
#pragma endregion

#pragma region Shader Module
		// Validate that the specified entry points exist in the shader module
		std::expected<void, std::string> validationResult = ValidateComputeEntryPoints(entryPoints, spec);
		if (!validationResult)
			return std::unexpected(validationResult.error());

		// Create shader modules and pipeline shader stage create info for each entry point
		vk::raii::ShaderModule shaderModule = spec.Shader->CreateModule(m_RenderDevice);

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { CreateShaderStage(shaderModule, entryPoints, spec.EntryPoint) };
#pragma endregion

#pragma region Compute Pipeline Creation
		vk::ComputePipelineCreateInfo pipelineCreateInfo{
			.stage = shaderStages[0],
			.layout = *m_GlobalLayout
		};
		auto pipeline = vk::raii::Pipeline(m_RenderDevice.GetDevice(), nullptr, pipelineCreateInfo);
		if (!spec.DebugName.empty())
			m_RenderDevice.SetDebugName(*pipeline, spec.DebugName);
#pragma endregion

		return PipelineEntry{
			.Pipeline{ std::move(pipeline) },
			.Spec{ spec }
		};
	}

	std::expected<void, std::string> PipelineStore::ValidateHandle(GPUPipelineHandle handle) const
	{
		if (!handle.IsValid())
			return std::unexpected("Pipeline handle is not valid!");
		if (handle.Index >= m_Pipelines.size())
			return std::unexpected("Pipeline entry no longer exists!");
		if (m_Pipelines[handle.Index].Pipeline == nullptr)
			return std::unexpected("Pipeline is not ready yet!");

		return {};
	}

}