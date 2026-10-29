#include "cdlpch.h"
#include "ShaderLoader.h"
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

		std::expected<std::vector<std::byte>, std::string> ReadShaderFile(const std::filesystem::path& filePath)
		{
			auto code = Platform::ReadFile(filePath);
			if (!code) {
				return std::unexpected(std::format("Failed to read shader file '{}': {} (OS error {})",
					ToUtf8(filePath), ToString(code.error().Code), code.error().OSError));
			}

			if (code->empty() || code->size() % sizeof(uint32_t) != 0) {
				return std::unexpected(std::format("Shader file '{}' is {} bytes; SPIR-V must be a non-empty multiple of 4",
					ToUtf8(filePath), code->size()));
			}

			return std::move(*code);
		}

		PushConstantBlock ExtractPushConstantBlock(spv_reflect::ShaderModule& shaderModule, std::string_view entryPointName)
		{
			const SpvReflectBlockVariable* pcBlock = shaderModule.GetEntryPointPushConstantBlock(entryPointName.data());
			if (pcBlock == nullptr) {
				return {};
			}

			PushConstantBlock pushConstantBlock;
			pushConstantBlock.Name = pcBlock->name ? pcBlock->name : "";
			pushConstantBlock.Size = pcBlock->size;
			for (uint32_t i = 0; i < pcBlock->member_count; ++i) {
				const SpvReflectBlockVariable& member = pcBlock->members[i];
				pushConstantBlock.Members.push_back({
					.Name = member.name ? member.name : "",
					.Size = member.size,
					.Offset = member.offset
					});
			}

			return pushConstantBlock;
		}

		std::expected<std::vector<ShaderEntryPoint>, std::string> ExtractShaderSourceInfo(std::span<const std::byte> shaderCode, const std::filesystem::path& filePath)
		{
			auto shaderModule = spv_reflect::ShaderModule(shaderCode.size(), shaderCode.data());

			if (shaderModule.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {
				return std::unexpected(std::format("Failed to reflect shader file '{}' (SpvReflectResult {})",
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

				std::string entryPointName = shaderModule.GetEntryPointName(i);
				entryPoints.push_back({ 
					.Name = entryPointName,
					.Stage = stage, 
					.PushConstants = std::move(ExtractPushConstantBlock(shaderModule, entryPointName))
				});
			}

			return entryPoints;
		}

	}

	std::expected<SharedPtr<Shader>, std::string> ShaderLoader::LoadShader(const std::string& name, const std::filesystem::path& filePath)
	{
		auto shaderCode = ReadShaderFile(filePath);
		if (!shaderCode)
			return std::unexpected(shaderCode.error());

		// ReadShaderFile guarantees at least one word; memcpy because the buffer holds bytes, not uint32_t objects.
		uint32_t magic = 0;
		std::memcpy(&magic, shaderCode->data(), sizeof(magic));
		if (magic != SpvMagicNumber)
			return std::unexpected(std::format("Shader file '{}' is not SPIR-V (bad magic number)", ToUtf8(filePath)));

		// Reflect first since it bounds-checks every instruction whereas creating vulkan shader module on malformed SPIR-V is undefined
		auto entryPoints = ExtractShaderSourceInfo(*shaderCode, filePath);
		if (!entryPoints)
			return std::unexpected(entryPoints.error());

		ShaderCreationInfo shaderInfo{
			.Name = name,
			.FilePath = filePath,
			.EntryPoints = std::move(*entryPoints)
		};

		return SharedPtr<Shader>::Create(std::move(shaderInfo), std::move(*shaderCode));
	}

}