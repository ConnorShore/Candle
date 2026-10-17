#pragma once

#include "RenderConstants.h"

#include <cstdint>

namespace Candle {

	struct ValidationSpecification
	{
#ifdef CDL_DEBUG
		bool EnableValidation = true;
#else
		bool EnableValidation = false;
#endif

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
		VulkanAPIVersion VulkanVersion{ kDefaultVulkanVersion };
		ValidationSpecification ValidationSpec = { };
		RenderDevicePreference DevicePreference{ RenderDevicePreference::DiscreteGPU };

		bool EnableVSync = true;
	};

}