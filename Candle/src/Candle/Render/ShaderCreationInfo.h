#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace Candle {

	enum class ShaderStage
	{
		Vertex = 0,
		Fragment = 1,
		Compute = 2,
		Count
	};

	// Reflection is per entry point, not per stage: one .spv holds every entry point of its source, and may hold several of one stage.
	struct ShaderEntryPoint
	{
		std::string Name;
		ShaderStage Stage;
		// In future will add more info such as descriptor sets, push constants, etc.
	};

	struct ShaderCreationInfo
	{
		std::string Name;
		std::filesystem::path FilePath;
		std::vector<ShaderEntryPoint> EntryPoints;
	};

}