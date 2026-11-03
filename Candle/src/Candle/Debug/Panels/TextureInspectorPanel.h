#pragma once

#include "Candle/Debug/DebugPanel.h"

namespace Candle {

	class TextureInspectorPanel : public DebugPanel
	{
	public:
		TextureInspectorPanel() = default;
		virtual ~TextureInspectorPanel() override = default;

		virtual void OnUpdate(const DebugContext& context) override;
		virtual void OnRender(const DebugContext& context) override;

		inline virtual const char* GetName() const override { return "Texture Inspector"; }
		inline virtual DebugPanelCategory GetCategory() const override { return DebugPanelCategory::Rendering; }
		inline virtual DebugPanelCommand GetCommand() const override
		{
			return DebugPanelCommand{
				.Name{ "TextureInspector" },
				.Description{ "Inspect textures in the texture pool" },
				.Usage{ "TextureInspector" }
			};
		}

	private:

	};

}