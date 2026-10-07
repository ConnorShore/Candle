#pragma once

#include "ShaderCreationInfo.h"

#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <vector>
#include <filesystem>

namespace Candle {

	// TODO: This will become an Asset/Resource in the future when the Asset/Resource managment system is implemented
	// Threading: not internally synchronized. Immutable once constructed, so any thread may read it after it has been handed over (e.g. across a job dependency)
	// Lifetime: destroy before the RenderDevice that created it, since the module is destroyed through that device
	class Shader
	{
	public:
		Shader(ShaderCreationInfo info, vk::raii::ShaderModule&& module);
		~Shader() = default;

		Shader(Shader&&) = default;
		Shader& operator=(Shader&&) = default;

		Shader(const Shader&) = delete;
		Shader& operator=(const Shader&) = delete;

		inline const std::string& GetName() const { return m_Name; }
		inline const std::filesystem::path& GetFilePath() const { return m_FilePath; }
		inline const vk::raii::ShaderModule& GetModule() const { return m_ShaderModule; }
		inline const std::vector<ShaderEntryPoint>& GetEntryPoints() const { return m_EntryPoints; }

	private:
		std::string m_Name;
		std::filesystem::path m_FilePath;
		vk::raii::ShaderModule m_ShaderModule{ nullptr };
		std::vector<ShaderEntryPoint> m_EntryPoints;
	};

}