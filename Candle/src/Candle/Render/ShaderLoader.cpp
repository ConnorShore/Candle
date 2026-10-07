#include "cdlpch.h"
#include "ShaderLoader.h"
#include "RenderDevice.h"
#include "Shader.h"

#include <spirv_reflect.h>

namespace Candle {

	namespace {

		ShaderStage ToShaderStage(SpvReflectShaderStageFlagBits stage)
		{
			switch (stage)
			{
			case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT: return ShaderStage::Vertex;
			case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT: return ShaderStage::Fragment;
			case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT: return ShaderStage::Compute;
			}

			CDL_CORE_ASSERT(false, "Unhandled SpvReflectShaderStageFlagBits");
			return ShaderStage::Count;
		}

		// TODO: Move this to some utility class (or maybe Platform)
		// and don't crash program if a load fails
		std::vector<uint32_t> ReadShaderFile(const std::filesystem::path filename) {
			std::ifstream file(filename, std::ios::ate | std::ios::binary);
			if (!file.is_open()) {
				throw std::runtime_error(std::format("Failed to open shader file '{}'", filename.string()));
			}

			// Checked here because this is the last point the byte count exists; dividing into words drops any remainder.
			const std::streamoff byteSize = file.tellg();
			if (byteSize <= 0 || byteSize % 4 != 0) {
				throw std::runtime_error(std::format("Shader file '{}' is {} bytes; SPIR-V must be a non-empty multiple of 4", filename.string(), byteSize));
			}

			std::vector<uint32_t> buffer(static_cast<size_t>(byteSize) / sizeof(uint32_t));

			file.seekg(0, std::ios::beg);
			if (!file.read(reinterpret_cast<char*>(buffer.data()), byteSize)) {
				throw std::runtime_error(std::format("Failed to read shader file '{}'", filename.string()));
			}

			return buffer;
		}

		std::vector<ShaderEntryPoint> ExtractShaderSourceInfo(const std::vector<uint32_t>& shaderCode, const std::filesystem::path& filePath)
		{
			auto shaderModule = spv_reflect::ShaderModule(shaderCode);

			// TODO: Eventually just return error and such
			if (shaderModule.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {
				throw std::runtime_error(std::format("Failed to reflect shader file '{}' (SpvReflectResult {})",
					filePath.string(), static_cast<int>(shaderModule.GetResult())));
			}

			std::vector<ShaderEntryPoint> entryPoints;
			entryPoints.reserve(shaderModule.GetEntryPointCount());
			for (uint32_t i = 0; i < shaderModule.GetEntryPointCount(); ++i) {
				ShaderStage stage = ToShaderStage(shaderModule.GetEntryPointShaderStage(i));
				if (stage < ShaderStage::Count)
					entryPoints.push_back({ .Stage = stage, .Name = shaderModule.GetEntryPointName(i) });

				// In future, handle descriptor sets, push constants, etc. here as well
			}

			return entryPoints;
		}

	}

	Shader ShaderLoader::LoadShader(const std::string& name, const std::filesystem::path& filePath, RenderDevice& device)
	{
		auto shaderCode = ReadShaderFile(filePath);

		// ReadShaderFile guarantees at least one word.
		if (shaderCode[0] != SpvMagicNumber) {
			throw std::runtime_error(std::format("Shader file '{}' is not SPIR-V (bad magic number)", filePath.string()));
		}

		// Reflect first since it bounds-checks every instruction whereas creating vulkan shader module on malformed SPIR-V is undefined
		ShaderCreationInfo shaderInfo{
			.Name = name,
			.FilePath = filePath,
			.EntryPoints = ExtractShaderSourceInfo(shaderCode, filePath)
		};

		vk::ShaderModuleCreateInfo createInfo{
			.codeSize = shaderCode.size() * sizeof(uint32_t),
			.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
		};
		vk::raii::ShaderModule shaderModule{
			device.GetDevice(),
			createInfo
		};

		return { std::move(shaderInfo), std::move(shaderModule) };
	}

}