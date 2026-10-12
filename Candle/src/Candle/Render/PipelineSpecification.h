#pragma once

#include "Shader.h"

#include "Candle/Core/Memory/SharedPtr.h"

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

	enum class BlendMode
	{
		None,
		SrcAlpha,
		OneMinusSrcAlpha,
		DstAlpha,
		OneMinusDstAlpha,
	};

	struct PipelineSpecification
	{
		SharedPtr<Shader> Shader{ nullptr };
		PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
		BlendMode Blend = BlendMode::None;
		std::vector<vk::Format> ColorAttachmentFormats;				// Must match color attachments duirng command buffer recording; empty means no color attachments
		vk::Format DepthAttachmentFormat = vk::Format::eUndefined;	// Must match depth attachment during command buffer recording; eUndefined means no depth attachment
		bool CullBackFaces = true;
	};

}