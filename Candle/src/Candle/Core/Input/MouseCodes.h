#pragma once

#include <cstdint>
#include <format>

namespace Candle {

	using MouseButtonType = uint8_t;

	enum class MouseButton : MouseButtonType
	{
		Unknown = 0,
		Left    = 1,
		Middle  = 2,
		Right   = 3,
		X1      = 4,    /* usually Back */
		X2      = 5,    /* usually Forward */

		Count   = 6
	};

}

template<>
struct std::formatter<Candle::MouseButton> : std::formatter<int>
{
	auto format(Candle::MouseButton mouseButton, std::format_context& ctx) const
	{
		return std::formatter<int>::format(static_cast<int>(mouseButton), ctx);
	}
};
