#include "cdlpch.h"
#include "Shader.h"

namespace Candle {

	Shader::Shader(const std::string& name, std::filesystem::path filePath, vk::raii::ShaderModule&& module)
		: m_Name(name), m_FilePath(filePath), m_ShaderModule(std::move(module))
	{
	}

}