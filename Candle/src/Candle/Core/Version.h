#pragma once

#include <string>

namespace Candle {

	struct VersionInfo
	{
		int Major = 1;
		int Minor = 0;
		int Patch = 0;

		std::string ToString() const
		{
			return std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Patch);
		}
	};

	constexpr VersionInfo kCandleVersion{ 1, 0, 0 };
}
