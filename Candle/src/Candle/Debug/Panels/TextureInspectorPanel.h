#pragma once

#include "Candle/Debug/DebugPanel.h"
#include "Candle/Render/Resources/GPUHandles.h"

#include <glm/glm.hpp>

#include <array>
#include <optional>

namespace Candle {

	class TexturePool;
	struct TextureStats;
	struct TexturePoolStats;

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
		inline virtual DebugPanelCommand GetCommand() override
		{
			return DebugPanelCommand{
				.Name{ "TextureInspector" },
				.Description{ "Inspect textures in the texture pool" },
				.Usage{ "TextureInspector" },
				.Callback{ nullptr }	// Opt for default callback to open/close for now (in future can pass in texture name to select)
			};
		}

	private:
		enum class TextureKind { Default, Texture, RenderTarget };	// In the order the Kind column sorts them

		void DrawSummaryRegion(const TexturePoolStats& stats);
		void DrawTextureList(const TexturePoolStats& stats);
		void DrawPreviewRegion(const TextureStats& texture);
		void HandleCanvasInput(glm::vec2 extent, glm::vec2 canvasMin, glm::vec2 canvasSize);	// Fit, wheel zoom and drag pan
		void DrawCanvas(GPUTextureHandle texture, glm::vec2 canvasMin, glm::vec2 canvasSize, glm::vec2 imageMin, glm::vec2 imageSize) const;

		static TextureKind KindOf(const TextureStats& texture);
		static const char* KindName(TextureKind kind);

	private:
		const TexturePool& m_TexturePool;
		GPUTextureHandle m_SelectedTexture;

		std::array<char, 64> m_NameFilter{};
		std::optional<TextureKind> m_KindFilter;	// Empty shows every kind

		bool m_FitToView{ true };	// Refits every frame, until a zoom or pan takes over
		float m_Scale{ 1.0f };		// Screen pixels per texel
		glm::vec2 m_Pan{ 0.0f };	// The image's offset from the canvas centre, in screen pixels
	};

}
