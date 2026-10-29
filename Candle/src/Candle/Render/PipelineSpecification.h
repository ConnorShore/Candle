#pragma once

#include "Shader.h"

#include "Candle/Core/Memory/SharedPtr.h"

#include <string>
#include <vector>

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

	struct PipelineSpecification
	{
		SharedPtr<Shader> Shader{ nullptr };
		PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
		BlendMode Blend = BlendMode::Opaque;
		std::vector<vk::Format> ColorAttachmentFormats;				// Must match color attachments duirng command buffer recording; empty means no color attachments
		vk::Format DepthAttachmentFormat = vk::Format::eUndefined;	// Must match depth attachment during command buffer recording; eUndefined means no depth attachment
		bool CullBackFaces = true;
		std::string DebugName;	// Names the pipeline and its layout; empty leaves them unnamed
	};

}