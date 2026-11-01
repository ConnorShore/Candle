#pragma once

#include "GuiDrawData.h"

#include <imgui.h>

namespace Candle {

	class GuiRender
	{
	public:

		void RenderDrawData(const GuiDrawData& drawData);
		void UpdateTexture(ImTextureData* textureData);

	private:

	};

}