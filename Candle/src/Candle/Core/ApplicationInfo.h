#pragma once

#include "Candle/Core/Version.h"

#include <string>

namespace Candle {

	struct ApplicationInfo
	{
		std::string Name = "Candle App";
		VersionInfo Version = { 1, 0, 0 };
	};

}
