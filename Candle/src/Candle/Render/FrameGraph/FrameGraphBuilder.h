#pragma once

#include "FrameGraphHandles.h"
#include "FrameGraphResources.h"

#include "Candle/Render/Vulkan/Image.h"

#include <string>

#include <vulkan/vulkan.hpp>

namespace Candle {

	class FrameGraph;
	struct FrameGraphPassNode;

	class FrameGraphBuilder
	{
	public:
		FrameGraphBuilder(FrameGraph& graph, uint32_t passIndex);
		~FrameGraphBuilder() = default;

		FGTextureHandle CreateTexture(std::string_view name, FGTextureSpec spec);
		//FGBufferHandle CreateBuffer(std::string_view name, FGBufferSpec spec);

		void ReadTexture(FGTextureHandle texture, FGUsage usage);
		void WriteTexture(FGTextureHandle texture, FGUsage usage);

		//void ReadBuffer(FGBufferHandle buffer, FGUsage usage);
		//void WriteBuffer(FGBufferHandle buffer, FGUsage usage);

		void ColorAttachment(FGTextureHandle texture, uint8_t binding, FGLoad loadOp, vk::ClearColorValue clearColor);
		void DepthAttachment(FGTextureHandle texture, FGLoad loadOp);

	private:
		FrameGraph& m_Graph;
		uint32_t m_PassIndex;
	};
}