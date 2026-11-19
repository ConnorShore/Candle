#include "cdlpch.h"
#include "FrameGraphResources.h"


namespace Candle {

	constexpr Candle::FGUsageInfo GetUsageInfo(FGUsage usage)
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
		case FGUsage::StorageVertex:
			return { .State = { .Layout = vk::ImageLayout::eGeneral, .Access = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
								.Stage = vk::PipelineStageFlagBits2::eVertexShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eStorage, .BufferUsage = {}, .Read = true, .Write = true };
		case FGUsage::StorageFragment:
			return { .State = { .Layout = vk::ImageLayout::eGeneral, .Access = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
								.Stage = vk::PipelineStageFlagBits2::eFragmentShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eStorage, .BufferUsage = {}, .Read = true, .Write = true };
		case FGUsage::StorageCompute:
			return { .State = { .Layout = vk::ImageLayout::eGeneral, .Access = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
								.Stage = vk::PipelineStageFlagBits2::eComputeShader },
					 .ImageUsage = vk::ImageUsageFlagBits::eStorage, .BufferUsage = {}, .Read = true, .Write = true };
		default:
			CDL_CORE_ASSERT(false, "Unknown FGUsage");
			return {};
	}

}