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
		vk::Format Format{ vk::Format::eUndefined };
	};

	struct FGBufferSpec
	{
		uint32_t Size{ 0 };
	};

	enum FGLoad { Load, Clear, DontCare };

	// FGUsage is a higher-level concept than Vulkan's image and buffer usage flags, so it can be used to describe both. 
	// It also describes the access pattern, which is needed for the frame graph to insert the right barriers.
	enum FGUsage
	{
		TransferSrc,
		TransferDst,
		ColorAttachment,
		DepthAttachment,
		SampledVertex,
		SampledFragment,
		SampledCompute,
		StorageReadVertex,
		StorageReadFragment,
		StorageReadCompute,
		StorageWriteVertex,
		StorageWriteFragment,
		StorageWriteCompute
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
	constexpr FGUsageInfo GetUsageInfo(FGUsage usage)
	{
		switch (usage)
		{
		case FGUsage::TransferSrc:
			return { .State = { .Layout = vk::ImageLayout::eTransferSrcOptimal, .Access = vk::AccessFlagBits2::eTransferRead, .Stage = vk::PipelineStageFlagBits2::eTransfer },
					 .ImageUsage = vk::ImageUsageFlagBits::eTransferSrc, .BufferUsage = vk::BufferUsageFlagBits::eTransferSrc, .Read = true, .Write = false };
		case FGUsage::TransferDst:
			return { .State = { .Layout = vk::ImageLayout::eTransferDstOptimal, .Access = vk::AccessFlagBits2::eTransferWrite, .Stage = vk::PipelineStageFlagBits2::eTransfer },
					 .ImageUsage = vk::ImageUsageFlagBits::eTransferDst, .BufferUsage = vk::BufferUsageFlagBits::eTransferDst, .Read = false, .Write = true };
		case FGUsage::ColorAttachment:
			return { .State = { .Layout = vk::ImageLayout::eColorAttachmentOptimal, .Access = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
								.Stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput },
					 .ImageUsage = vk::ImageUsageFlagBits::eColorAttachment, .BufferUsage = {}, .Read = true, .Write = true };
		case FGUsage::DepthAttachment:
			return { .State = { .Layout = vk::ImageLayout::eDepthStencilAttachmentOptimal, .Access = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
								.Stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests },
					 .ImageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment, .BufferUsage = {}, .Read = true, .Write = true };
		case FGUsage::SampledVertex:
			return { .State = { .Layout = vk::ImageLayout::eShaderReadOnlyOptimal, .Access = vk::AccessFlagBits2::eShaderSampledRead,
								.Stage = vk::PipelineStageFlagBits2::eVertexShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eSampled, .BufferUsage = {}, .Read = true, .Write = false };
		case FGUsage::SampledFragment:
			return { .State = { .Layout = vk::ImageLayout::eShaderReadOnlyOptimal, .Access = vk::AccessFlagBits2::eShaderSampledRead,
								.Stage = vk::PipelineStageFlagBits2::eFragmentShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eSampled, .BufferUsage = {}, .Read = true, .Write = false };
		case FGUsage::SampledCompute:
			return { .State = { .Layout = vk::ImageLayout::eShaderReadOnlyOptimal, .Access = vk::AccessFlagBits2::eShaderSampledRead,
								.Stage = vk::PipelineStageFlagBits2::eComputeShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eSampled, .BufferUsage = {}, .Read = true, .Write = false };
		case FGUsage::StorageReadVertex:
			return { .State = { .Layout = vk::ImageLayout::eGeneral, .Access = vk::AccessFlagBits2::eShaderStorageRead, .Stage = vk::PipelineStageFlagBits2::eVertexShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eStorage, .BufferUsage = {}, .Read = true, .Write = false };
		case FGUsage::StorageReadFragment:
			return { .State = { .Layout = vk::ImageLayout::eGeneral, .Access = vk::AccessFlagBits2::eShaderStorageRead, .Stage = vk::PipelineStageFlagBits2::eFragmentShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eStorage, .BufferUsage = {}, .Read = true, .Write = false };
		case FGUsage::StorageReadCompute:
			return { .State = { .Layout = vk::ImageLayout::eGeneral, .Access = vk::AccessFlagBits2::eShaderStorageRead, .Stage = vk::PipelineStageFlagBits2::eComputeShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eStorage, .BufferUsage = {}, .Read = true, .Write = false };
		// The writes keep read access too, since the graph can't tell whether the shader reads before it writes
		case FGUsage::StorageWriteVertex:
			return { .State = { .Layout = vk::ImageLayout::eGeneral, .Access = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
								.Stage = vk::PipelineStageFlagBits2::eVertexShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eStorage, .BufferUsage = {}, .Read = true, .Write = true };
		case FGUsage::StorageWriteFragment:
			return { .State = { .Layout = vk::ImageLayout::eGeneral, .Access = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
								.Stage = vk::PipelineStageFlagBits2::eFragmentShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eStorage, .BufferUsage = {}, .Read = true, .Write = true };
		case FGUsage::StorageWriteCompute:
			return { .State = { .Layout = vk::ImageLayout::eGeneral, .Access = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
								.Stage = vk::PipelineStageFlagBits2::eComputeShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eStorage, .BufferUsage = {}, .Read = true, .Write = true };
		default:
			CDL_CORE_ASSERT(false, "Unknown FGUsage");
			return {};
		}
	}
}