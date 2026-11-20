#pragma once

#include "FrameGraphResources.h"
#include "FrameGraph.h"

namespace Candle {

	struct FGBarrier
	{
		FGTextureHandle Texture;
		ImageState FromState;
		ImageState ToState;
	};

	struct FGCompiledAttachment
	{
		FGTextureHandle Texture;
		vk::ImageLayout Layout;
		vk::AttachmentLoadOp LoadOp;
		vk::AttachmentStoreOp StoreOp;
		vk::ClearValue ClearValue;
	};

	struct FGCompiledPass
	{
		std::string_view Name;
		uint32_t PassIndex;	// TODO: May not need but keeping till culling is implemented
		uint32_t BarrierStartIndex, BarrierCount;
		uint32_t AttachmentStartIndex, AttachmentCount;	// Colors first then depth as last
		bool HasDepthAttachment{ false };
	};

	struct FGCompiledGraph
	{
		std::vector<FGBarrier> Barriers;
		std::vector<FGCompiledPass> Passes;
		std::vector<FGCompiledAttachment> Attachments;
		std::vector<FGBarrier> FinalBarriers;	// For imported textures, to transition them to their final state for the next frame

		void Reset()
		{
			Barriers.clear();
			Passes.clear();
			Attachments.clear();
			FinalBarriers.clear();
		}
	};

}
