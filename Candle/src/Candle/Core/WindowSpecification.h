#pragma once

#include <string>
#include <cstdint>

namespace Candle {

	enum class WindowMode
	{
		Windowed,
		BorderlessFullscreen
	};

	struct WindowSpecification
	{
		std::string Title = "Candle App";
		uint32_t Width = 1600;		// Width/Height are the windowed size; BorderlessFullscreen takes the display's
		uint32_t Height = 900;
		WindowMode Mode = WindowMode::Windowed;
	};

}