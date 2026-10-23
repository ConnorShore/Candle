// AssetManager with stand-in assets: storage by name and type, ownership, and routing to the registered handler.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "AssetTestHelpers.h"

#include <Candle/Asset/AssetManager.h>

#include <algorithm>
#include <string_view>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	struct RecordingManager
	{
		std::vector<TextureStub*> Loaded;
		std::vector<TextureStub*> Unloaded;
		AssetManager Manager;

		RecordingManager() { Manager.RegisterAssetHandler(ScopedPtr<TextureStubHandler>::Create(Loaded, Unloaded)); }
	};

}

CDL_TEST_CASE(AssetManager, LoadPassesTheAssetToItsTypesHandler, Unit)
{
	RecordingManager test;
	SharedPtr<TextureStub> texture = SharedPtr<TextureStub>::Create();

	test.Manager.LoadAsset("Brick", texture);

	CDL_CHECK_EQ(test.Loaded.size(), 1u);
	CDL_EXPECT(test.Loaded[0] == texture.Get());
	CDL_EXPECT(test.Unloaded.empty());
}

CDL_TEST_CASE(AssetManager, LoadSkipsHandlersOfOtherTypes, Unit)
{
	RecordingManager test;

	test.Manager.LoadAsset("Brick", SharedPtr<MaterialStub>::Create());

	CDL_EXPECT(test.Loaded.empty());
}

// Loading without a handler is a setup mistake worth a warning, not a reason to lose the asset.
CDL_TEST_CASE(AssetManager, LoadWithoutAHandlerStoresTheAssetAndWarns, Unit)
{
	LoggerFixture logs(LogLevel::Warn, static_cast<uint16_t>(LogChannel::Asset));
	{
		AssetManager manager;
		SharedPtr<TextureStub> texture = SharedPtr<TextureStub>::Create();

		manager.LoadAsset("Brick", texture);

		CDL_EXPECT(manager.GetAsset<TextureStub>("Brick") == texture);
		CDL_CHECK(Logger::Flush());
	}

	const bool warned = std::ranges::any_of(logs.Records, [](const LogRecord& record) {
		return record.Level == LogLevel::Warn && record.Channel == LogChannel::Asset
			&& std::string_view(record.Message).contains("No asset handler");
	});
	CDL_EXPECT(warned);
}

CDL_TEST_CASE(AssetManager, FindsAnAssetByNameAndByTheIdLoadReturned, Unit)
{
	AssetManager manager;
	SharedPtr<TextureStub> texture = SharedPtr<TextureStub>::Create();

	const uint64_t id = manager.LoadAsset("Brick", texture);

	CDL_EXPECT(manager.GetAsset<TextureStub>("Brick") == texture);
	CDL_EXPECT(manager.GetAsset(id).Get() == texture.Get());
}

CDL_TEST_CASE(AssetManager, ReturnsNullForAnUnknownName, Unit)
{
	AssetManager manager;
	manager.LoadAsset("Brick", SharedPtr<TextureStub>::Create());

	CDL_EXPECT(!manager.GetAsset<TextureStub>("Stone"));
}

// The id hashes the type in, so a texture and a material may share a name.
CDL_TEST_CASE(AssetManager, OneNameUnderTwoTypesIsTwoAssets, Unit)
{
	AssetManager manager;
	SharedPtr<TextureStub> texture = SharedPtr<TextureStub>::Create();
	SharedPtr<MaterialStub> material = SharedPtr<MaterialStub>::Create();

	const uint64_t textureId = manager.LoadAsset("Brick", texture);
	const uint64_t materialId = manager.LoadAsset("Brick", material);

	CDL_EXPECT_NE(textureId, materialId);
	CDL_EXPECT(manager.GetAsset<TextureStub>("Brick") == texture);
	CDL_EXPECT(manager.GetAsset<MaterialStub>("Brick") == material);
}

CDL_TEST_CASE(AssetManager, LoadingANameAgainReplacesTheAsset, Unit)
{
	AssetManager manager;
	SharedPtr<TextureStub> first = SharedPtr<TextureStub>::Create(1);
	SharedPtr<TextureStub> second = SharedPtr<TextureStub>::Create(2);

	const uint64_t firstId = manager.LoadAsset("Brick", first);
	const uint64_t secondId = manager.LoadAsset("Brick", second);

	CDL_EXPECT_EQ(firstId, secondId);
	CDL_EXPECT(manager.GetAsset<TextureStub>("Brick") == second);
	CDL_EXPECT_EQ(first->GetRefCount(), 1u);	// The manager let go of it
}

CDL_TEST_CASE(AssetManager, KeepsALoadedAssetAlive, Unit)
{
	AssetManager manager;
	{
		SharedPtr<TextureStub> texture = SharedPtr<TextureStub>::Create(7);
		manager.LoadAsset("Brick", texture);
	}

	SharedPtr<TextureStub> found = manager.GetAsset<TextureStub>("Brick");
	CDL_CHECK(found);
	CDL_EXPECT_EQ(found->Id, 7);
}

CDL_TEST_CASE(AssetManager, UnloadPassesTheAssetToItsHandlerAndLetsGoOfIt, Unit)
{
	RecordingManager test;
	SharedPtr<TextureStub> texture = SharedPtr<TextureStub>::Create();
	test.Manager.LoadAsset("Brick", texture);

	test.Manager.UnloadAsset("Brick", AssetType::Texture);

	CDL_CHECK_EQ(test.Unloaded.size(), 1u);
	CDL_EXPECT(test.Unloaded[0] == texture.Get());
	CDL_EXPECT(!test.Manager.GetAsset<TextureStub>("Brick"));
	CDL_EXPECT_EQ(texture->GetRefCount(), 1u);
}

// The type is part of the id, so unloading the texture leaves a material of the same name loaded.
CDL_TEST_CASE(AssetManager, UnloadLeavesTheSameNameUnderAnotherType, Unit)
{
	RecordingManager test;
	SharedPtr<MaterialStub> material = SharedPtr<MaterialStub>::Create();
	test.Manager.LoadAsset("Brick", SharedPtr<TextureStub>::Create());
	test.Manager.LoadAsset("Brick", material);

	test.Manager.UnloadAsset("Brick", AssetType::Texture);

	CDL_EXPECT(!test.Manager.GetAsset<TextureStub>("Brick"));
	CDL_EXPECT(test.Manager.GetAsset<MaterialStub>("Brick") == material);
}

// Unloading what was never loaded is a caller mistake worth a warning; no handler sees it and nothing else is touched.
CDL_TEST_CASE(AssetManager, UnloadingAnUnknownNameWarnsAndChangesNothing, Unit)
{
	LoggerFixture logs(LogLevel::Warn, static_cast<uint16_t>(LogChannel::Asset));
	{
		RecordingManager test;
		SharedPtr<TextureStub> texture = SharedPtr<TextureStub>::Create();
		test.Manager.LoadAsset("Brick", texture);

		test.Manager.UnloadAsset("Stone", AssetType::Texture);

		CDL_EXPECT(test.Unloaded.empty());
		CDL_EXPECT(test.Manager.GetAsset<TextureStub>("Brick") == texture);
		CDL_CHECK(Logger::Flush());
	}

	const bool warned = std::ranges::any_of(logs.Records, [](const LogRecord& record) {
		return record.Level == LogLevel::Warn && record.Channel == LogChannel::Asset
			&& std::string_view(record.Message).contains("No loaded asset");
	});
	CDL_EXPECT(warned);
}

// The mirror of loading without a handler: a warning, but the manager still lets go of the asset.
CDL_TEST_CASE(AssetManager, UnloadWithoutAHandlerStillForgetsTheAssetAndWarns, Unit)
{
	LoggerFixture logs(LogLevel::Warn, static_cast<uint16_t>(LogChannel::Asset));
	{
		AssetManager manager;
		manager.LoadAsset("Brick", SharedPtr<TextureStub>::Create());

		manager.UnloadAsset("Brick", AssetType::Texture);

		CDL_EXPECT(!manager.GetAsset<TextureStub>("Brick"));
		CDL_CHECK(Logger::Flush());
	}

	const auto handlerWarnings = std::ranges::count_if(logs.Records, [](const LogRecord& record) {
		return record.Level == LogLevel::Warn && std::string_view(record.Message).contains("No asset handler");
	});
	CDL_EXPECT_EQ(handlerWarnings, 2);	// One for the load, one for the unload
}

CDL_TEST_CASE(AssetManager, ASecondHandlerForATypeReplacesTheFirst, Unit)
{
	std::vector<TextureStub*> firstLoaded, secondLoaded, unloaded;
	AssetManager manager;
	manager.RegisterAssetHandler(ScopedPtr<TextureStubHandler>::Create(firstLoaded, unloaded));
	manager.RegisterAssetHandler(ScopedPtr<TextureStubHandler>::Create(secondLoaded, unloaded));

	manager.LoadAsset("Brick", SharedPtr<TextureStub>::Create());

	CDL_EXPECT(firstLoaded.empty());
	CDL_EXPECT_EQ(secondLoaded.size(), 1u);
}
