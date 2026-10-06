#pragma once

#include <string>

namespace Candle {

	struct VersionInfo
	{
		uint8_t Major = 1;
		uint8_t Minor = 0;
		uint8_t Patch = 0;

		std::string ToString() const
		{
			return std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Patch);
		}
	};

	constexpr VersionInfo kCandleVersion{ 1, 0, 0 };
}
