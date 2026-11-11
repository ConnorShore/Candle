#pragma once

#include <string>
#include <filesystem>

#include "Candle/Core/ApplicationInfo.h"
#include "Candle/Core/Logger.h"
#include "Candle/Platform/WindowSpecification.h"
#include "Candle/Render/RenderSpecification.h"

namespace Candle {

	struct ApplicationSpecification
	{
		ApplicationInfo AppInfo;

		// A relative directory resolves against the executable's directory, never the working directory
		std::filesystem::path EngineAssetDir = CDL_ENGINE_ASSET_DIR;
		std::filesystem::path ProjectAssetDir = "GameData";
		std::filesystem::path SavedDir = "Saved";	// What the engine writes for this machine at runtime, like the pipeline cache

		int CommandLineArgsCount = 0;
		char** CommandLineArgs = nullptr;

		bool Headless = false;			// No window, windowing backend or event pump; WindowSpec is ignored

		WindowSpecification WindowSpec;
		RenderSpecification RenderSpec;
		LoggerSpecification LoggerSpec;
	};

}