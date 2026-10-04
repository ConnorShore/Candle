#pragma once

#include <string>
#include <filesystem>

#include "Candle/Core/Logger.h"
#include "Candle/Core/WindowSpecification.h"

namespace Candle {

	struct ApplicationSpecification
	{
		std::string Name = "Candle App";

		std::filesystem::path EngineAssetDir = "CandleCore";
		std::filesystem::path ProjectAssetDir = "GameData";

		int CommandLineArgsCount = 0;
		char** CommandLineArgs = nullptr;

		bool Headless = false;			// No window, windowing backend or event pump; WindowSpec is ignored
		WindowSpecification WindowSpec;
		LoggerSpecification LoggerSpec;
	};

}