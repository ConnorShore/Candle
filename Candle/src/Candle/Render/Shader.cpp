#include "cdlpch.h"
#include "Shader.h"

namespace Candle {

	Shader::Shader(ShaderCreationInfo info, vk::raii::ShaderModule&& module)
		: m_Name(std::move(info.Name)), m_FilePath(std::move(info.FilePath)), m_ShaderModule(std::move(module)), m_EntryPoints(std::move(info.EntryPoints))
	{
	}

}