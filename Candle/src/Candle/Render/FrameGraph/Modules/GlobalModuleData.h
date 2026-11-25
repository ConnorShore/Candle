#pragma once

#include "Candle/Render/FrameGraph/FrameGraphHandles.h"

namespace Candle {

	struct FrameTargetData
	{
		FGTextureHandle SwapChain;
		FGTextureHandle Output;
	};

	struct SceneTextureData
	{
		FGTextureHandle Color;
		FGTextureHandle Depth;
	};

	struct FrameGraphPreviewData
	{
		FGTextureHandle Target;
	};

}