#pragma once

#include "Candle/Debug/DebugPanel.h"
#include "Candle/Render/Resources/GPUHandles.h"

namespace Candle {

	class TexturePool;

	// Threading: main thread only, which is what lets it read the pool's live handles and debug names
	class TextureInspectorPanel : public DebugPanel
	{
	public:
		explicit TextureInspectorPanel(const TexturePool& texturePool) : m_TexturePool(texturePool) {}
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
		const TexturePool& m_TexturePool;
		GPUTextureHandle m_SelectedTexture;
	};

}