#pragma once

#include "Candle/Render/RenderConstants.h"

#include <cstdint>

namespace Candle {

	struct ValidationSpecification
	{
#ifdef CDL_DEBUG
		bool EnableValidation = true;
#else
		bool EnableValidation = false;
#endif
		bool EnableSynchronizationValidation = true;	// Only applies with validation on

		// Object names and command buffer labels for RenderDoc and Nsight. Always on with validation, which needs it.
#ifdef CDL_DIST
		bool EnableDebugUtils = false;
#else
		bool EnableDebugUtils = true;
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