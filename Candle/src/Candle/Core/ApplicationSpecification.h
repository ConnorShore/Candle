#pragma once

#include <string>
#include <filesystem>

#include "Candle/Core/Logger.h"
#include "Candle/Core/WindowSpecification.h"
#include "Candle/Render/RenderSpecification.h"

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

	struct ApplicationInfo
	{
		std::string Name = "Candle App";
		VersionInfo Version = { 1, 0, 0 };
	};

	struct ApplicationSpecification
	{
		ApplicationInfo AppInfo;

		std::filesystem::path EngineAssetDir = "CandleCore";
		std::filesystem::path ProjectAssetDir = "GameData";

		int CommandLineArgsCount = 0;
		char** CommandLineArgs = nullptr;

		bool Headless = false;			// No window, windowing backend or event pump; WindowSpec is ignored

		WindowSpecification WindowSpec;
		RenderSpecification RenderSpec;
		LoggerSpecification LoggerSpec;
	};

}