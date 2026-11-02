// ToGuiTextureID, the way a texture handle reaches ImGui: both halves of the handle packed into one ImTextureID. The way
// back is private to GuiSystem, so GuiSystemTests checks the round trip through ImGui.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Render/GUI/GuiTextureID.h>

#include <cstdint>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

// Distinct bits in every byte, so a swapped or truncated half shows.
CDL_TEST_CASE(GuiTextureID, PacksTheIndexLowAndTheGenerationHigh, Unit)
{
	CDL_EXPECT_EQ(ToGuiTextureID({ .Index = 0x9ABCDEF0, .Generation = 0x12345678 }), uint64_t{ 0x123456789ABCDEF0 });
}

// The generation is what tells a live texture from one that reused its slot, so it must survive into the ID.
CDL_TEST_CASE(GuiTextureID, HandlesDifferingInEitherHalfGetDistinctIDs, Unit)
{
	CDL_EXPECT_NE(ToGuiTextureID({ .Index = 7, .Generation = 1 }), ToGuiTextureID({ .Index = 7, .Generation = 2 }));
	CDL_EXPECT_NE(ToGuiTextureID({ .Index = 7, .Generation = 1 }), ToGuiTextureID({ .Index = 8, .Generation = 1 }));
}
