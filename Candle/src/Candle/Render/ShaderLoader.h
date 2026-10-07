#pragma once

#include <string>
#include <filesystem>

namespace Candle {

	class Shader;
	class RenderDevice;

	class ShaderLoader
	{
	public:
		static Shader LoadShader(const std::string& name, const std::filesystem::path& filePath, RenderDevice& device);
	};

}