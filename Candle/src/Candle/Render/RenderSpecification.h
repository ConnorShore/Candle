#pragma once

namespace Candle {

	enum class VulkanAPIVersion
	{
		API_1_3,
		API_1_4
	};

	struct ValidationSpecification
	{
		bool EnableValidation = true;

		enum class ValidationSeverity : uint8_t
		{
			Verbose = 1 << 0,
			Info	= 1 << 1,
			Warning = 1 << 2,
			Error	= 1 << 3
		} Severity = ValidationSeverity::Warning;

		enum class ValidationType : uint8_t
		{
			General			= 1 << 0,
			Validation		= 1 << 1,
			Performance		= 1 << 2,
			All				= General | Validation | Performance
		} Type = ValidationType::All;

	};

	struct RenderSpecification
	{
		VulkanAPIVersion VulkanVersion = VulkanAPIVersion::API_1_4;
		ValidationSpecification ValidationSpec = { };

		bool EnableVSync = true;
	};

}