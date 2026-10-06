#include "cdlpch.h"
#include "ShaderLoader.h"
#include "Shader.h"
#include "RenderDevice.h"

namespace Candle {

	namespace {

		// TODO: Move this to some utility class (or maybe Platform)
		// and don't crash program if a load fails
		std::vector<uint32_t> readFile(const std::filesystem::path filename) {
			std::ifstream file(filename, std::ios::ate | std::ios::binary);
			if (!file.is_open()) {
				throw std::runtime_error("failed to open file!");
			}

			std::vector<uint32_t> buffer(file.tellg() / sizeof(uint32_t));

			file.seekg(0, std::ios::beg);
			file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size() * sizeof(uint32_t)));
			file.close();

			return buffer;
		}

	}

	Shader ShaderLoader::LoadShader(const std::string& name, std::filesystem::path filePath, RenderDevice& device)
	{
		auto shaderCode = readFile(filePath);

		CDL_CORE_ASSERT(shaderCode.size() % 4 == 0, "Shader code size must be a multiple of 4 bytes");
		CDL_CORE_ASSERT(shaderCode[0] == 0x07230203, "Invalid SPIR-V shader code");

		vk::ShaderModuleCreateInfo createInfo{
			.codeSize = shaderCode.size() * sizeof(uint32_t),
			.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
		};
		vk::raii::ShaderModule shaderModule{
			device.GetDevice(),
			createInfo
		};

		return { name, filePath, std::move(shaderModule) };
	}

}