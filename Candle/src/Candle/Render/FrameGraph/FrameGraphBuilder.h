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

		[[nodiscard]] FGTextureHandle CreateTexture(std::string_view name, FGTextureSpec spec);
		//FGBufferHandle CreateBuffer(std::string_view name, FGBufferSpec spec);

		void ReadTexture(FGTextureHandle texture, FGUsage usage);
		[[nodiscard]] FGTextureHandle WriteTexture(FGTextureHandle texture, FGUsage usage);

		//void ReadBuffer(FGBufferHandle buffer, FGUsage usage);
		//void WriteBuffer(FGBufferHandle buffer, FGUsage usage);

		[[nodiscard]] FGTextureHandle ColorAttachment(FGTextureHandle texture, FGLoad loadOp, vk::ClearColorValue clearColor);
		[[nodiscard]] FGTextureHandle DepthAttachment(FGTextureHandle texture, FGLoad loadOp);

	private:
		uint32_t IncrementHandleVersion(FGTextureHandle handle);

	private:
		FrameGraph& m_Graph;
		uint32_t m_PassIndex;
	};
}