// AssetHandler's typed dispatch and the IsAsset concept it is constrained on.

#include "TestFramework.h"
#include "AssetTestHelpers.h"

#include <Candle/Asset/Asset.h>
#include <Candle/Asset/AssetHandler.h>

#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	struct AssetWithoutType : Asset
	{
		AssetType GetType() const override { return AssetType::Shader; }
	};

	struct TypeWithoutAsset
	{
		inline static constexpr AssetType kType = AssetType::Shader;
	};

}

// An asset needs both the base class and a kType for the handler to report.
CDL_STATIC_ASSERT(IsAsset<TextureStub>);
CDL_STATIC_ASSERT(!IsAsset<AssetWithoutType>);
CDL_STATIC_ASSERT(!IsAsset<TypeWithoutAsset>);
CDL_STATIC_ASSERT(!IsAsset<int>);

CDL_TEST_CASE(AssetHandler, ReportsItsAssetsType, Unit)
{
	std::vector<TextureStub*> loaded, unloaded;
	TextureStubHandler handler(loaded, unloaded);

	CDL_EXPECT_EQ(handler.GetAssetType(), AssetType::Texture);
}

// AssetManager only sees IAssetHandler, so the untyped entry points must reach the typed overloads with the same object.
CDL_TEST_CASE(AssetHandler, UntypedCallsReachTheTypedOverloads, Unit)
{
	std::vector<TextureStub*> loaded, unloaded;
	TextureStubHandler handler(loaded, unloaded);
	IAssetHandler& untyped = handler;

	TextureStub texture;
	untyped.OnLoad(texture);
	CDL_CHECK_EQ(loaded.size(), 1u);
	CDL_EXPECT(loaded[0] == &texture);
	CDL_EXPECT(unloaded.empty());

	untyped.OnUnload(texture);
	CDL_CHECK_EQ(unloaded.size(), 1u);
	CDL_EXPECT(unloaded[0] == &texture);
	CDL_EXPECT_EQ(loaded.size(), 1u);
}
