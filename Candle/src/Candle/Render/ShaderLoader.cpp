#include "cdlpch.h"
#include "ShaderLoader.h"
#include "RenderDevice.h"
#include "Shader.h"

#include "Candle/Platform/Platform.h"

#include <spirv_reflect.h>

#include <cstring>
#include <span>

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

		std::string ToUtf8(const std::filesystem::path& path)
		{
			const std::u8string utf8 = path.u8string();
			return { reinterpret_cast<const char*>(utf8.data()), utf8.size() };
		}

		// TODO: Don't crash program if a load fails
		std::vector<std::byte> ReadShaderFile(const std::filesystem::path& filePath)
		{
			auto code = Platform::ReadFile(filePath);
			if (!code) {
				throw std::runtime_error(std::format("Failed to read shader file '{}': {} (OS error {})",
					ToUtf8(filePath), ToString(code.error().Code), code.error().OSError));
			}

			if (code->empty() || code->size() % sizeof(uint32_t) != 0) {
				throw std::runtime_error(std::format("Shader file '{}' is {} bytes; SPIR-V must be a non-empty multiple of 4",
					ToUtf8(filePath), code->size()));
			}

			return std::move(*code);
		}

		std::vector<ShaderEntryPoint> ExtractShaderSourceInfo(std::span<const std::byte> shaderCode, const std::filesystem::path& filePath)
		{
			auto shaderModule = spv_reflect::ShaderModule(shaderCode.size(), shaderCode.data());

			// TODO: Eventually just return error and such
			if (shaderModule.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {
				throw std::runtime_error(std::format("Failed to reflect shader file '{}' (SpvReflectResult {})",
					ToUtf8(filePath), static_cast<int>(shaderModule.GetResult())));
			}

			std::vector<ShaderEntryPoint> entryPoints;
			entryPoints.reserve(shaderModule.GetEntryPointCount());
			for (uint32_t i = 0; i < shaderModule.GetEntryPointCount(); ++i) {
				ShaderStage stage = ToShaderStage(shaderModule.GetEntryPointShaderStage(i));

				// If unsupported stage, warn and skip it.
				if (stage == ShaderStage::Count) {
					CDL_CORE_WARN(LogChannel::Render, "Shader file '{}' has an entry point '{}' with an unhandled stage (SpvReflectShaderStageFlagBits {})",
						ToUtf8(filePath), shaderModule.GetEntryPointName(i), static_cast<int>(shaderModule.GetEntryPointShaderStage(i)));
					continue;
				}

				// In future, handle descriptor sets, push constants, etc. here as well

				entryPoints.push_back({ .Name = shaderModule.GetEntryPointName(i), .Stage = stage });
			}

			return entryPoints;
		}

	}

	SharedPtr<Shader> ShaderLoader::LoadShader(const std::string& name, const std::filesystem::path& filePath, RenderDevice& device)
	{
		std::vector<std::byte> shaderCode = ReadShaderFile(filePath);

		// ReadShaderFile guarantees at least one word; memcpy because the buffer holds bytes, not uint32_t objects.
		uint32_t magic = 0;
		std::memcpy(&magic, shaderCode.data(), sizeof(magic));
		if (magic != SpvMagicNumber) {
			throw std::runtime_error(std::format("Shader file '{}' is not SPIR-V (bad magic number)", ToUtf8(filePath)));
		}

		// Reflect first since it bounds-checks every instruction whereas creating vulkan shader module on malformed SPIR-V is undefined
		ShaderCreationInfo shaderInfo{
			.Name = name,
			.FilePath = filePath,
			.EntryPoints = ExtractShaderSourceInfo(shaderCode, filePath)
		};

		return SharedPtr<Shader>::Create(std::move(shaderInfo), std::move(shaderCode));
	}

}