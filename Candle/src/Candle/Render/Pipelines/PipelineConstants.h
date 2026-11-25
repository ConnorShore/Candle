#pragma once

namespace Candle {

	// The one push-constant range every pipeline layout declares, so pushed values survive pipeline binds.
	// All stages (compute included, once it shares layouts); 128 bytes is the maxPushConstantsSize every device guarantees.
	inline constexpr vk::ShaderStageFlags kPushConstantStages = vk::ShaderStageFlagBits::eAll;
	inline constexpr uint32_t kPushConstantsSize = 128;

}