#pragma once

#include <string>
#include <cstdint>

namespace Candle {

	enum class WindowMode
	{
		Windowed,
		Borderless,
		Fullscreen
	};

	struct WindowSpecification
	{
		std::string Title = "Candle App";
		uint32_t Width = 1600;
		uint32_t Height = 900;
		WindowMode Mode = WindowMode::Windowed;
	};

}