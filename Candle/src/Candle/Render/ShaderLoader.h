#pragma once

#include "Candle/Core/Memory/SharedPtr.h"

#include <string>
#include <filesystem>
#include <expected>

namespace Candle {

	class Shader;

	class ShaderLoader
	{
	public:
		// Threading: any thread, concurrently.
		// Blocking (a file read), so never on the main or render thread mid-frame.
		static std::expected<SharedPtr<Shader>, std::string> LoadShader(const std::string& name, const std::filesystem::path& filePath);
	};

}