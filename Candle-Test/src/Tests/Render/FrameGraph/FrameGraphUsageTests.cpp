// GetUsageInfo is the one table the builder and compiler read every access through, so each row has to agree with how they use
// it, including the usages no pass exercises yet.

#include "TestFramework.h"

#include <Candle/Render/FrameGraph/FrameGraphResources.h>

#include <format>
#include <string>
#include <utility>

using namespace Candle;
using Candle::Test::Type::Unit;

namespace {

	constexpr FGUsage kEveryUsage[] = {
		FGUsage::TransferSrc, FGUsage::TransferDst, FGUsage::ColorAttachment, FGUsage::DepthAttachment,
		FGUsage::SampledVertex, FGUsage::SampledFragment, FGUsage::SampledCompute,
		FGUsage::StorageReadVertex, FGUsage::StorageReadFragment, FGUsage::StorageReadCompute,
		FGUsage::StorageWriteVertex, FGUsage::StorageWriteFragment, FGUsage::StorageWriteCompute };

	constexpr vk::AccessFlags2 kWriteAccesses = vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eColorAttachmentWrite
		| vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eTransferWrite;

	std::string UsageLabel(FGUsage usage)
	{
		return std::format("FGUsage {}", std::to_underlying(usage));
	}

}

// Compile builds a transient's image from the usages its passes declare and skips one left with none as unused, and a barrier
// into no stage waits for nothing
CDL_TEST_CASE(FrameGraphUsage, EveryUsageNamesAnImageUsageAndAStage, Unit)
{
	for (const FGUsage usage : kEveryUsage)
	{
		const FGUsageInfo info = GetUsageInfo(usage);
		CDL_EXPECT_MSG(info.ImageUsage != vk::ImageUsageFlags{}, UsageLabel(usage));
		CDL_EXPECT_MSG(info.State.Stage != vk::PipelineStageFlags2{}, UsageLabel(usage));
	}
}

// The barrier after a write takes its source access from the write's state, so a write usage without a write bit would leave
// what it wrote unflushed. ReadTexture and WriteTexture go by Write, so the flag has to say the same thing.
CDL_TEST_CASE(FrameGraphUsage, WritesAreExactlyTheUsagesWithAWriteAccess, Unit)
{
	for (const FGUsage usage : kEveryUsage)
	{
		const FGUsageInfo info = GetUsageInfo(usage);
		const bool hasWriteAccess = static_cast<bool>(info.State.Access & kWriteAccesses);
		CDL_EXPECT_MSG(info.Write == hasWriteAccess, UsageLabel(usage));
	}
}

// Compile swaps in the texture's own layout for sampled reads but takes storage layouts from here, and BindlessTable writes every
// storage descriptor in General
CDL_TEST_CASE(FrameGraphUsage, StorageUsagesAreInGeneralLikeTheirDescriptors, Unit)
{
	for (const FGUsage usage : kEveryUsage)
	{
		const FGUsageInfo info = GetUsageInfo(usage);
		if (info.ImageUsage & vk::ImageUsageFlagBits::eStorage)
			CDL_EXPECT_MSG(info.State.Layout == vk::ImageLayout::eGeneral, UsageLabel(usage));
	}
}
