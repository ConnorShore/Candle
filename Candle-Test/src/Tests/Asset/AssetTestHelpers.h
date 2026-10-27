#pragma once

// Stand-in asset types and a recording handler, so the asset system is tested without any renderer.

#include <Candle/Asset/Asset.h>
#include <Candle/Asset/AssetHandler.h>

#include <vector>

namespace Candle::Test {

	// Two asset types, so a test can load one name under both.
	struct TextureStub : Asset
	{
		explicit TextureStub(const std::string& name, int id = 0) : Asset(name, {}), Id(id) {}

		inline static constexpr AssetType kType = AssetType::Texture;
		AssetType GetType() const override { return kType; }

		int Id;
	};

	struct MaterialStub : Asset
	{
		explicit MaterialStub(const std::string& name) : Asset(name, {}) {}

		inline static constexpr AssetType kType = AssetType::Material;
		AssetType GetType() const override { return kType; }
	};

	// Appends every asset it is handed; the vectors belong to the test, since the manager takes the handler.
	class TextureStubHandler : public AssetHandler<TextureStub>
	{
	public:
		TextureStubHandler(std::vector<TextureStub*>& loaded, std::vector<TextureStub*>& unloaded)
			: m_Loaded(loaded), m_Unloaded(unloaded) {}

	protected:
		void Load(TextureStub& asset) override { m_Loaded.push_back(&asset); }
		void Unload(TextureStub& asset) override { m_Unloaded.push_back(&asset); }

	private:
		std::vector<TextureStub*>& m_Loaded;
		std::vector<TextureStub*>& m_Unloaded;
	};

}
