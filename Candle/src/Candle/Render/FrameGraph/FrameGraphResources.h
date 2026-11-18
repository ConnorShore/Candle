#pragma once

#include "Candle/Render/Vulkan/Image.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace Candle {

	// TODO: Most of these will move to their own files, just keeping here for simplicity while getting the frame graph
	//  setup and working
	enum class FGSizeMode { RelativeToOutput, Absolute };
	struct FGSize
	{
		FGSizeMode Mode;
		glm::uvec2 Extent;
		float Scale{ 1.0f };
	};

	struct FGTextureSpec
	{
		FGSize Size;

		// TOOD: Maybe move to own format spec
		vk::Format Format{ vk::Format::eUndefined };
	};

	struct FGBufferSpec
	{
		uint32_t Size{ 0 };
	};

	enum FGUsage
	{
		TransferSrc,
		TransferDst,
		ColorAttachment,
		DepthAttachment,
		SampledVertex,
		SampledFragment,
		SampledCompute,
		StorageVertex,
		StorageFragment,
		StorageCompute
	};

	struct FGUsageInfo
	{
		ImageState State;
		vk::ImageUsageFlags ImageUsage;
		vk::BufferUsageFlags BufferUsage;
		bool Read{ false };
		bool Write{ false };
	};

	// Will be a table that maps to different usages
	constexpr FGUsageInfo GetUsageInfo(FGUsage usage);

	enum FGLoad { Load, Clear, DontCare };
}