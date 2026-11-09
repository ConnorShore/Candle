#pragma once

#include "Candle/Render/Pipelines/Shader.h"

#include "Candle/Core/Memory/SharedPtr.h"

#include <string>
#include <vector>
#include <variant>

namespace Candle {

	// Maps directly to Vulkan's vk::PrimitiveTopology
	enum class PrimitiveTopology
	{
		PointList = 0,
		LineList,
		LineStrip,
		TriangleList,
		TriangleStrip,
		TriangleFan,
	};

	// Applied to every color attachment
	enum class BlendMode
	{
		Opaque,
		AlphaBlend,
		PremultipliedAlpha,
		Additive,
	};

	struct GraphicsPipelineSpecification
	{
		SharedPtr<Shader> Shader{ nullptr };
		std::string VertexEntryPoint{ "VertMain" };
		std::string FragmentEntryPoint{ "FragMain" };
		PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
		BlendMode Blend = BlendMode::Opaque;
		std::vector<vk::Format> ColorAttachmentFormats;				// Must match color attachments duirng command buffer recording; empty means no color attachments
		vk::Format DepthAttachmentFormat = vk::Format::eUndefined;	// Must match depth attachment during command buffer recording; eUndefined means no depth attachment
		bool CullBackFaces = true;
		std::string DebugName;	// Names the pipeline; empty leaves them unnamed
	};

	struct ComputePipelineSpecification
	{
		SharedPtr<Shader> Shader{ nullptr };
		std::string EntryPoint{ "CompMain" };
		std::string DebugName;	// Names the pipeline; empty leaves them unnamed
	};

	using PipelineSpecification = std::variant<GraphicsPipelineSpecification, ComputePipelineSpecification>;

}