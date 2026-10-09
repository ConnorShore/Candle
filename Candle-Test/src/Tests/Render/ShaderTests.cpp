// Shader as a value type: it takes its creation info whole and is move-only, so a VkShaderModule has one owner.
// A null module keeps these headless; ShaderLoaderTests covers a real one.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Render/Shader.h>

#include <string>
#include <type_traits>
#include <utility>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

// A copy would be a second owner destroying the same VkShaderModule.
CDL_STATIC_ASSERT(!std::is_copy_constructible_v<Shader>);
CDL_STATIC_ASSERT(!std::is_copy_assignable_v<Shader>);
CDL_STATIC_ASSERT(std::is_move_constructible_v<Shader>);
CDL_STATIC_ASSERT(std::is_move_assignable_v<Shader>);

namespace {

	ShaderCreationInfo LitInfo()
	{
		return {
			.Name = "Lit",
			.FilePath = "shaders/Lit.spv",
			.EntryPoints = {
				{.Name = "vertMain", .Stage = ShaderStage::Vertex },
				{.Name = "fragMain", .Stage = ShaderStage::Fragment },
			},
		};
	}

	void ExpectLit(const Shader& shader)
	{
		CDL_EXPECT_EQ(shader.GetName(), std::string("Lit"));
		CDL_EXPECT(shader.GetFilePath() == std::filesystem::path("shaders/Lit.spv"));
		CDL_CHECK_EQ(shader.GetEntryPoints().size(), 2u);
		CDL_EXPECT_EQ(shader.GetEntryPoints()[0].Stage, ShaderStage::Vertex);
		CDL_EXPECT_EQ(shader.GetEntryPoints()[0].Name, std::string("vertMain"));
		CDL_EXPECT_EQ(shader.GetEntryPoints()[1].Stage, ShaderStage::Fragment);
		CDL_EXPECT_EQ(shader.GetEntryPoints()[1].Name, std::string("fragMain"));
	}

}
