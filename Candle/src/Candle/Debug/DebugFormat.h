#pragma once

#include <cstdint>
#include <format>
#include <string>

namespace Candle {

	inline std::string FormatBytes(uint64_t bytes)
	{
		constexpr uint64_t kKilobyte = 1024;
		constexpr uint64_t kMegabyte = 1024 * kKilobyte;
		constexpr uint64_t kGigabyte = 1024 * kMegabyte;

		if (bytes >= kGigabyte)
			return std::format("{:.2f} GB", static_cast<double>(bytes) / static_cast<double>(kGigabyte));
		if (bytes >= kMegabyte)
			return std::format("{:.1f} MB", static_cast<double>(bytes) / static_cast<double>(kMegabyte));
		if (bytes >= kKilobyte)
			return std::format("{:.1f} KB", static_cast<double>(bytes) / static_cast<double>(kKilobyte));
		return std::format("{} B", bytes);
	}

}
