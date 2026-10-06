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

		// A minimum, like LogLevel: Warning reports warnings and errors. Ordered, so it compares as a threshold.
		enum class ValidationSeverity : uint8_t
		{
			Verbose,
			Info,
			Warning,
			Error
		} Severity = ValidationSeverity::Warning;

		enum class ValidationType : uint8_t
		{
			General			= 1 << 0,
			Validation		= 1 << 1,
			Performance		= 1 << 2,
			All				= General | Validation | Performance
		} Type = ValidationType::All;

	};

	enum class RenderDevicePreference
	{
		DiscreteGPU,
		IntegratedGPU,
		Any
	};

	struct RenderSpecification
	{
		VulkanAPIVersion VulkanVersion = VulkanAPIVersion::API_1_4;
		ValidationSpecification ValidationSpec = { };
		RenderDevicePreference DevicePreference = RenderDevicePreference::DiscreteGPU;

		bool EnableVSync = true;
	};

}