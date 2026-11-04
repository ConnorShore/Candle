#pragma once

#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <cstdint>

namespace Candle {

	inline std::string ToLower(std::string_view str)
	{
		std::string lowerStr(str);
		std::ranges::transform(lowerStr, lowerStr.begin(), [](unsigned char c) {
			return std::tolower(c);
			});

		return lowerStr;
	}

	inline uint64_t HashStringToUUID(std::string_view str)
	{
		// TODO: Use a better hashing algorithm, this is just a placeholder for now
		std::hash<std::string_view> hasher;
		return hasher(str);
	}

}