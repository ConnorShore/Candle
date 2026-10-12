#pragma once

#include "Candle/Core/Memory/SharedPtr.h"

#include <string>
#include <filesystem>

namespace Candle {

	class Shader;
	class RenderDevice;

	class ShaderLoader
	{
	public:
		// Threading: any thread, concurrently.
		// Blocking (a file read plus the driver's module creation), so never on the main or render thread mid-frame.
		static SharedPtr<Shader> LoadShader(const std::string& name, const std::filesystem::path& filePath, RenderDevice& device);
	};

}