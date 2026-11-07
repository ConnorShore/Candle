#pragma once

#include "Candle/Platform/Platform.h"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>
#include <cstdlib>

#include "Candle/Core/Logger.h"

namespace Candle::ErrorHandler {

	namespace {

		void WriteNVidiaFaultInfoToFile(const std::string& outputFileName, std::span<std::byte> data)
		{
			if (data.empty())
			{
				CDL_CORE_WARN(LogChannel::Render, "No vendor binary data available to write to file.");
				return;
			}

			// Remove first 56 bytes of the binary data, which is the header, to avoid writing it to the file
			std::filesystem::path outputPath = outputFileName + ".nv-gpudmp";
			auto ret = Platform::WriteFile(outputPath, { data.data() + 56, data.size() - 56 });
			if (!ret)
			{
				CDL_CORE_ERROR(LogChannel::Render, "Failed to write NVidia device fault binary data to {}: {}", outputPath.string(), static_cast<int>(ret.error().Code));
				return;
			}

			CDL_CORE_INFO(LogChannel::Render, "Device fault NVidia binary data written to {}", outputPath.string());
		}

	}

	void CheckDeviceFault(vk::raii::Device const& device) 
	{
		vk::DeviceFaultCountsEXT faultCounts{};
		vk::Result result = device.getFaultInfoEXT(&faultCounts, nullptr);   // counts only
		if (result != vk::Result::eSuccess && result != vk::Result::eIncomplete)
		{
			CDL_CORE_ERROR(LogChannel::Render, "Failed to retrieve device fault information: {}", vk::to_string(result));
			return;
		}

		std::vector<vk::DeviceFaultAddressInfoEXT> addresses(faultCounts.addressInfoCount);
		std::vector<vk::DeviceFaultVendorInfoEXT> vendorInfos(faultCounts.vendorInfoCount);
		std::vector<std::byte> binaryData(faultCounts.vendorBinarySize);

		vk::DeviceFaultInfoEXT info{
			.pAddressInfos = addresses.data(),
			.pVendorInfos = vendorInfos.data(),
			.pVendorBinaryData = binaryData.data()
		};

		result = device.getFaultInfoEXT(&faultCounts, &info);
		if (result != vk::Result::eSuccess && result != vk::Result::eIncomplete) 
		{
			CDL_CORE_ERROR(LogChannel::Render, "Failed to retrieve device fault information: {}", vk::to_string(result));
			return;
		}

		if (faultCounts.addressInfoCount > 0 || faultCounts.vendorInfoCount > 0)
		{
			CDL_CORE_ERROR(LogChannel::Render, "Device Fault encountered.");

			// Check general fault description
			if (info.description[0] != '\0')
				CDL_CORE_ERROR(LogChannel::Render, "Fault Description: {}", info.description.data());

			// Log vendor-specific information if available
			if (faultCounts.vendorInfoCount > 0 && info.pVendorInfos != nullptr)
				CDL_CORE_ERROR(LogChannel::Render, "Vendor Fault Description: {}", info.pVendorInfos->description.data());
			else
				CDL_CORE_ERROR(LogChannel::Render, "Vendor Fault Description: No Vendor Information available.");

			// Log each address info using std::span (C++20) or pointer indexing
			for (uint32_t i = 0; i < faultCounts.addressInfoCount; ++i)
			{
				const auto& addrInfo = info.pAddressInfos[i];
				CDL_CORE_ERROR(LogChannel::Render, "Fault Address Info Address Type: {}", vk::to_string(addrInfo.addressType));
				CDL_CORE_ERROR(LogChannel::Render, "Fault Address Info Reported Address -> Decimal: {} | Hex: 0x{:X}",
					addrInfo.reportedAddress,
					addrInfo.reportedAddress);
			}
		}
		else
		{
			CDL_CORE_INFO(LogChannel::Render, "No device faults detected.");
		}

		// Write out the binary data to a file for further analysis if available
		if (faultCounts.vendorBinarySize > 0 && !binaryData.empty())
		{
			WriteNVidiaFaultInfoToFile("device_fault_info", binaryData);
		}
	}

}
