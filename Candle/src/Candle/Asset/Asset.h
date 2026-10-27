#pragma once

#include "Candle/Core/Memory/SharedPtr.h"

#include <string>
#include <filesystem>

namespace Candle {

	enum class AssetType
	{
		Mesh,
		Texture,
		Material,
		Model,
		Shader
	};
	// Placeholder for now
	class Asset : public SharedResource
	{
	public:
		Asset(const std::string& name, const std::filesystem::path& filePath)
			: m_Name(name), m_FilePath(filePath) { }
		virtual ~Asset() = default;

		virtual AssetType GetType() const = 0;

		inline const std::string& GetName() const { return m_Name; }
		inline const std::filesystem::path& GetFilePath() const { return m_FilePath; }

	protected:
		std::string m_Name;
		std::filesystem::path m_FilePath;

	private:

	};

	template<typename T>
	concept IsAsset = std::derived_from<T, Asset>
		&& requires { { T::kType } -> std::convertible_to<AssetType>; };
}