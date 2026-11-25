#include "cdlpch.h"
#include "RenderModule.h"

#include "Candle/Render/Pipelines/Shader.h"
#include "Candle/Render/Pipelines/ShaderLoader.h"

namespace Candle {

	SharedPtr<Shader> LoadEngineShader(const std::string& name, const std::filesystem::path& filePath)
	{
		auto shader = ShaderLoader::LoadShader(name, filePath);
		if (!shader)
			CDL_CORE_PANIC(LogChannel::Render, "{}", shader.error());

		return std::move(*shader);
	}

}
