#include "cdlpch.h"
#include "Shader.h"
#include "RenderDevice.h"

namespace Candle {

	Shader::Shader(ShaderCreationInfo info, std::vector<std::byte>&& shaderCode)
		: m_Name(std::move(info.Name))
		, m_FilePath(std::move(info.FilePath))
		, m_EntryPoints(std::move(info.EntryPoints))
		, m_ShaderCode(std::move(shaderCode))
	{
	}

	const vk::raii::ShaderModule Shader::CreateModule(RenderDevice& renderDevice)
	{
		// pCode must be 4-byte aligned; the vector's storage comes from operator new, which aligns to at least this.
		CDL_STATIC_ASSERT(__STDCPP_DEFAULT_NEW_ALIGNMENT__ >= alignof(uint32_t));
		vk::ShaderModuleCreateInfo createInfo{
			.codeSize = m_ShaderCode.size(),
			.pCode = reinterpret_cast<const uint32_t*>(m_ShaderCode.data())
		};

		vk::raii::ShaderModule shaderModule{
			renderDevice.GetDevice(),
			createInfo
		};

		return shaderModule;
	}

}