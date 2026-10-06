#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <filesystem>

namespace Candle {

	// TODO: This will become an Asset/Resource in the future when the Asset/Resource managment system is implemented
	class Shader
	{
	public:
		Shader(const std::string& name, std::filesystem::path filePath, vk::raii::ShaderModule&& module);
		~Shader() = default;

		Shader(Shader&&) = default;
		Shader& operator=(Shader&&) = default;

		Shader(const Shader&) = delete;
		Shader& operator=(const Shader&) = delete;

		inline const std::string& GetName() const { return m_Name; }
		inline const std::filesystem::path GetFilePath() const { return m_FilePath; }

	private:
		std::string m_Name;
		std::filesystem::path m_FilePath;
		vk::raii::ShaderModule m_ShaderModule{ nullptr };
	};

}