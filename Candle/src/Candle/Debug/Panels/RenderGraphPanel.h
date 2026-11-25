#pragma once

#include "Candle/Debug/DebugPanel.h"
#include "Candle/Render/FrameGraph/FrameGraphSnapshot.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Candle {

	class FrameGraphCapture;

	// The compiled frame graph as a matrix of passes against the textures they read and write, with the selected transient's contents.
	// Asks for a snapshot only while it renders, so a closed or hidden panel costs nothing.
	// Threading: main thread only.
	class RenderGraphPanel : public DebugPanel
	{
	public:
		explicit RenderGraphPanel(FrameGraphCapture& capture) : m_Capture(capture) {}
		virtual ~RenderGraphPanel() override = default;

		virtual void OnRender(const DebugContext& context) override;

		inline virtual const char* GetName() const override { return "Render Graph"; }
		inline virtual DebugPanelCategory GetCategory() const override { return DebugPanelCategory::Rendering; }
		inline virtual DebugPanelCommand GetCommand() override
		{
			return DebugPanelCommand{
				.Name{ "RenderGraph" },
				.Description{ "Inspect the compiled frame graph's passes, textures and transients" },
				.Usage{ "RenderGraph" },
				.Callback{ nullptr }	// Opt for default callback to open/close for now
			};
		}

	private:
		// The first and last live passes to touch a texture
		struct Lifetime
		{
			uint32_t First{ FrameGraphSnapshot::kNone };
			uint32_t Last{ 0 };

			bool IsUsed() const { return First != FrameGraphSnapshot::kNone; }
			bool Contains(uint32_t pass) const { return IsUsed() && First <= pass && pass <= Last; }
			bool Overlaps(const Lifetime& other) const { return IsUsed() && other.IsUsed() && First <= other.Last && other.First <= Last; }
		};

		void TakeLatestSnapshot();
		void RebuildCells();
		void RequestCapture();

		void DrawToolbar();
		void DrawLegend();
		void DrawMatrix();
		bool DrawCell(uint32_t texture, uint32_t pass);	// True when hovered
		void DrawAccessTooltip(uint32_t texture, uint32_t pass, const FGAccessEntry& access);
		void DrawDetails();
		void DrawPassDetails(uint32_t pass);
		void DrawTextureDetails(uint32_t texture);
		void DrawPreview(uint32_t texture);

		void SelectPass(uint32_t pass);
		void SelectTexture(uint32_t texture);

		std::optional<uint32_t> FindPassIndex(const std::string& name) const;
		std::optional<uint32_t> FindTextureIndex(const std::string& name) const;
		uint32_t CellAccess(uint32_t texture, uint32_t pass) const;
		const FGBarrier* FindBarrier(uint32_t pass, uint32_t texture) const;
		std::string LiveReason(uint32_t pass) const;
		bool IsPreviewable(uint32_t texture) const;

	private:
		FrameGraphCapture& m_Capture;
		FrameGraphSnapshot m_Snapshot;	// What's on screen: the newest to come back, or the one paused on
		bool m_HasSnapshot{ false };

		std::vector<Lifetime> m_Lifetimes;		// Per texture
		std::vector<uint32_t> m_CellAccesses;	// Per texture then pass: the access the matrix shows there, or kNone
		std::vector<uint32_t> m_Columns;		// The passes shown, rebuilt every frame from the toggles
		std::vector<uint32_t> m_Rows;			// The textures, in the order shown
		std::optional<uint32_t> m_HoveredTexture;	// Last frame's, for the overlap highlight

		std::string m_SelectedPass;		// By name, so a selection survives indices shifting between frames
		std::string m_SelectedTexture;
		glm::vec2 m_PreviewRange{ 0.0f, 1.0f };
		bool m_RecapturePreview{ false };	// A range change while paused, drawn again once

		bool m_Paused{ false };
		bool m_ShowCulled{ true };
		bool m_HighlightOverlaps{ false };
		bool m_SortByLifetime{ false };
	};

}
