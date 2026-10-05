#pragma once

namespace Candle {

	enum class VulkanAPIVersion
	{
		API_1_3,
		API_1_4
	};

	struct RenderSpecification
	{
		bool EnableVSync = true;
		bool EnableValidation = true;

		VulkanAPIVersion VulkanVersion = VulkanAPIVersion::API_1_4;
	};

}