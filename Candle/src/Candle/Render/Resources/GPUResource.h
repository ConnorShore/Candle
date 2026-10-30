#pragma once

#include "Candle/Render/Vulkan/GPUBuffer.h"
#include "Candle/Render/Pipelines/Pipeline.h"
#include "Candle/Render/Vulkan/SwapChainImage.h"

#include <variant>

namespace Candle {

	// The Vulkan object kinds a DeletionQueue can hold; each destroys itself when dropped
	using GPUResource = std::variant<GPUBuffer, Pipeline, SwapChainImage>;

}
