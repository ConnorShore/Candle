#pragma once

#include "GPUBuffer.h"
#include "Pipeline.h"
#include "SwapChainImage.h"

#include <variant>

namespace Candle {

	// The Vulkan object kinds a DeletionQueue can hold; each destroys itself when dropped
	using GPUResource = std::variant<GPUBuffer, Pipeline, SwapChainImage>;

}
