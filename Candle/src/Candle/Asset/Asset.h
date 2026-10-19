#pragma once

#include "Candle/Core/Memory/SharedPtr.h"

namespace Candle {

	enum class AssetType
	{
		Mesh,
		Texture,
		Material,
		Shader
	};
	// Placeholder for now
	class Asset : public SharedResource
	{
	public:

		virtual AssetType GetType() const = 0;

	protected:


	private:

	};

	template<typename T>
	concept IsAsset = std::derived_from<T, Asset>
		&& requires { { T::kType } -> std::convertible_to<AssetType>; };
}