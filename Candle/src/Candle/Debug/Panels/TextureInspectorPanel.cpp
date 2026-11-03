#include "cdlpch.h"
#include "TextureInspectorPanel.h"
#include "Candle/Render/Resources/TexturePool.h"
#include "Candle/Render/GUI/GuiTextureID.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <compare>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace Candle {

	namespace {

		constexpr float kListWidthFraction = 0.45f;	// The texture list's share of the window
		constexpr float kThumbnailSize = 128.0f;		// The hover tooltip's preview
		constexpr float kMinCanvasSize = 50.0f;			// InvisibleButton asserts on a zero size
		constexpr float kZoomStep = 1.2f;				// Per wheel notch
		constexpr float kMinScale = 1.0f / 64.0f;
		constexpr float kMaxScale = 64.0f;
		constexpr float kCheckerSize = 8.0f;

		constexpr ImU32 kCanvasColor = IM_COL32(15, 15, 15, 200);
		constexpr ImU32 kCheckerDarkColor = IM_COL32(60, 60, 60, 255);
		constexpr ImU32 kCheckerLightColor = IM_COL32(90, 90, 90, 255);

		enum class Column : ImGuiID { Index, Name, Kind, Size };

		ImVec2 ToImVec2(glm::vec2 v) { return { v.x, v.y }; }
		glm::vec2 ToVec2(ImVec2 v) { return { v.x, v.y }; }

		std::string FormatBytes(uint64_t bytes)
		{
			if (bytes >= 1024 * 1024)
				return std::format("{:.1f} MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
			if (bytes >= 1024)
				return std::format("{:.1f} KB", static_cast<double>(bytes) / 1024.0);
			return std::format("{} B", bytes);
		}

		bool ContainsIgnoringCase(std::string_view text, std::string_view pattern)
		{
			auto equal = [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); };
			return pattern.empty() || !std::ranges::search(text, pattern, equal).empty();
		}

		// The largest scale that fits the whole extent in bounds
		float FitScale(glm::vec2 extent, glm::vec2 bounds)
		{
			return std::min(bounds.x / extent.x, bounds.y / extent.y);
		}

	}

	void TextureInspectorPanel::OnUpdate(const DebugContext& context)
	{

	}

	void TextureInspectorPanel::OnRender(const DebugContext& context)
	{
		ImGui::SetNextWindowSize({ 900.0f, 500.0f }, ImGuiCond_FirstUseEver);
		if (ImGui::Begin(GetName(), &IsOpen))
		{
			// Only captured while the window is showing, since it copies every live texture's name
			const TexturePoolStats stats = m_TexturePool.CaptureStats();
			DrawSummaryRegion(stats);
			ImGui::Separator();

			ImGui::BeginChild("##TextureList", { ImGui::GetContentRegionAvail().x * kListWidthFraction, 0.0f });
			DrawTextureList(stats);
			ImGui::EndChild();

			ImGui::SameLine();
			ImGui::BeginChild("##TexturePreview", { 0.0f, 0.0f }, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

			// Only a texture in this frame's capture is alive, and a released handle must never reach the packet
			const auto selected = std::ranges::find(stats.ActiveTextures, m_SelectedTexture, &TextureStats::Handle);
			if (selected != stats.ActiveTextures.end())
			{
				DrawPreviewRegion(*selected);
			}
			else
			{
				m_SelectedTexture = {};
				ImGui::BeginDisabled();
				ImGui::TextWrapped("Select a texture to preview it");
				ImGui::EndDisabled();
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}

	void TextureInspectorPanel::DrawSummaryRegion(const TexturePoolStats& stats)
	{
		uint64_t totalBytes = 0;
		for (const TextureStats& texture : stats.ActiveTextures)
			totalBytes += texture.SizeBytes;

		const GPUResourcePoolStats& pool = stats.PoolStats;
		ImGui::Text("%zu textures, %s", stats.ActiveTextures.size(), FormatBytes(totalBytes).c_str());
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 4.0f);
		ImGui::TextDisabled("Live %u  Pending %u  Free %u", pool.LiveSlots, pool.PendingSlots, pool.FreeSlots);
		ImGui::SetItemTooltip("Pending slots are released but not yet back on the free list, since a frame in flight may still use them");

		ImGui::SetNextItemWidth(200.0f);
		ImGui::InputTextWithHint("##NameFilter", "Filter by name", m_NameFilter.data(), m_NameFilter.size());
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150.0f);
		if (ImGui::BeginCombo("##KindFilter", m_KindFilter ? KindName(*m_KindFilter) : "All kinds"))
		{
			if (ImGui::Selectable("All kinds", !m_KindFilter))
				m_KindFilter.reset();

			for (TextureKind kind : { TextureKind::Texture, TextureKind::RenderTarget, TextureKind::Default })
			{
				if (ImGui::Selectable(KindName(kind), m_KindFilter == kind))
					m_KindFilter = kind;
			}
			ImGui::EndCombo();
		}
	}

	void TextureInspectorPanel::DrawTextureList(const TexturePoolStats& stats)
	{
		// Pointers into this frame's capture, so they are rebuilt every frame
		const std::string_view nameFilter = m_NameFilter.data();
		std::vector<const TextureStats*> visible;
		visible.reserve(stats.ActiveTextures.size());
		for (const TextureStats& texture : stats.ActiveTextures)
		{
			if ((!m_KindFilter || KindOf(texture) == *m_KindFilter) && ContainsIgnoringCase(texture.DebugName, nameFilter))
				visible.push_back(&texture);
		}

		constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;
		if (!ImGui::BeginTable("##Textures", 4, kTableFlags))
			return;

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort, 0.0f, static_cast<ImGuiID>(Column::Index));
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, static_cast<ImGuiID>(Column::Name));
		ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 0.0f, static_cast<ImGuiID>(Column::Kind));
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, static_cast<ImGuiID>(Column::Size));
		ImGui::TableHeadersRow();

		// Sorted every frame rather than when the specs change, since the capture is rebuilt every frame.
		// Stable, so rows that tie stay in slot order.
		if (const ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs(); sortSpecs && sortSpecs->SpecsCount > 0)
		{
			const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
			auto compare = [column = static_cast<Column>(spec.ColumnUserID)](const TextureStats& a, const TextureStats& b) -> std::strong_ordering
			{
				switch (column)
				{
				case Column::Name: return a.DebugName <=> b.DebugName;
				case Column::Kind: return KindOf(a) <=> KindOf(b);
				case Column::Size: return a.SizeBytes <=> b.SizeBytes;
				default: return a.Handle.Index <=> b.Handle.Index;
				}
			};

			const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
			std::ranges::stable_sort(visible, [&](const TextureStats* a, const TextureStats* b) { return ascending ? compare(*a, *b) < 0 : compare(*a, *b) > 0; });
		}

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(visible.size()));
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				const TextureStats& texture = *visible[i];
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::PushID(static_cast<int>(texture.Handle.Index));

				if (ImGui::Selectable(std::to_string(texture.Handle.Index).c_str(), m_SelectedTexture == texture.Handle, ImGuiSelectableFlags_SpanAllColumns))
				{
					m_SelectedTexture = texture.Handle;
					m_FitToView = true;
				}

				if (ImGui::BeginItemTooltip())
				{
					const glm::vec2 extent(texture.Extent);
					ImGui::Image(ToGuiTextureID(texture.Handle), ToImVec2(extent * FitScale(extent, glm::vec2(kThumbnailSize))));
					ImGui::Text("%u x %u, %s", texture.Extent.x, texture.Extent.y, vk::to_string(texture.Format).c_str());
					ImGui::EndTooltip();
				}

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(texture.DebugName.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(KindName(KindOf(texture)));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(FormatBytes(texture.SizeBytes).c_str());

				ImGui::PopID();
			}
		}
		ImGui::EndTable();
	}

	void TextureInspectorPanel::DrawPreviewRegion(const TextureStats& texture)
	{
		// TODO: R/G/B/A channel toggles and a mip selector, alongside mip map support. Gui.slang only multiplies by the vertex colour,
		//  so it needs a view field in GuiDrawPushConstants, set per image through an ImGui callback that ExtractDrawData turns into data
		if (ImGui::Button("Fit"))
			m_FitToView = true;
		ImGui::SameLine();
		if (ImGui::Button("1:1"))
		{
			m_FitToView = false;
			m_Scale = 1.0f;
			m_Pan = glm::vec2(0.0f);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("Wheel to zoom, drag to pan, double-click to fit");

		// Leaves room below for the two lines of details
		const float detailsHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f;
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const glm::vec2 canvasMin = ToVec2(ImGui::GetCursorScreenPos());
		const glm::vec2 canvasSize = glm::max(glm::vec2(available.x, available.y - detailsHeight), glm::vec2(kMinCanvasSize));

		// Claims the area for the wheel and drags; the image is drawn into it after
		ImGui::InvisibleButton("##Canvas", ToImVec2(canvasSize));

		const glm::vec2 extent(texture.Extent);
		HandleCanvasInput(extent, canvasMin, canvasSize);

		const glm::vec2 imageSize = extent * m_Scale;
		const glm::vec2 imageMin = canvasMin + (canvasSize - imageSize) * 0.5f + m_Pan;
		DrawCanvas(texture.Handle, canvasMin, canvasSize, imageMin, imageSize);

		ImGui::TextUnformatted(texture.DebugName.c_str());
		ImGui::SameLine();
		ImGui::TextDisabled("%s", KindName(KindOf(texture)));
		ImGui::TextDisabled("%u x %u, %s, %u mip%s, %s, slot %u generation %u", texture.Extent.x, texture.Extent.y, vk::to_string(texture.Format).c_str(),
			texture.MipLevels, texture.MipLevels == 1 ? "" : "s", FormatBytes(texture.SizeBytes).c_str(), texture.Handle.Index, texture.Handle.Generation);
	}

	void TextureInspectorPanel::HandleCanvasInput(glm::vec2 extent, glm::vec2 canvasMin, glm::vec2 canvasSize)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			m_FitToView = true;

		// Kept up to date while fitting, so a zoom or pan carries on from what is on screen
		if (m_FitToView)
		{
			m_Scale = FitScale(extent, canvasSize);
			m_Pan = glm::vec2(0.0f);
		}

		const glm::vec2 mouseDelta = ToVec2(io.MouseDelta);
		if (ImGui::IsItemActive() && mouseDelta != glm::vec2(0.0f))
		{
			m_Pan += mouseDelta;
			m_FitToView = false;
		}

		if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f)
		{
			// About the cursor, so the texel under it stays put
			const float scale = std::clamp(m_Scale * std::pow(kZoomStep, io.MouseWheel), kMinScale, kMaxScale);
			const glm::vec2 imageCentre = canvasMin + canvasSize * 0.5f + m_Pan;
			m_Pan += (ToVec2(io.MousePos) - imageCentre) * (1.0f - scale / m_Scale);
			m_Scale = scale;
			m_FitToView = false;
		}
	}

	void TextureInspectorPanel::DrawCanvas(GPUTextureHandle texture, glm::vec2 canvasMin, glm::vec2 canvasSize, glm::vec2 imageMin, glm::vec2 imageSize) const
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const glm::vec2 canvasMax = canvasMin + canvasSize;
		drawList->AddRectFilled(ToImVec2(canvasMin), ToImVec2(canvasMax), kCanvasColor);

		// The checkerboard only covers the part of the image on screen, so a deep zoom doesn't draw thousands of squares
		const glm::vec2 visibleMin = glm::max(imageMin, canvasMin);
		const glm::vec2 visibleMax = glm::min(imageMin + imageSize, canvasMax);
		if (visibleMin.x < visibleMax.x && visibleMin.y < visibleMax.y)
		{
			drawList->PushClipRect(ToImVec2(visibleMin), ToImVec2(visibleMax), true);
			drawList->AddRectFilled(ToImVec2(visibleMin), ToImVec2(visibleMax), kCheckerDarkColor);
			for (int row = 0; visibleMin.y + row * kCheckerSize < visibleMax.y; ++row)
			{
				for (int column = row % 2; visibleMin.x + column * kCheckerSize < visibleMax.x; column += 2)
				{
					const glm::vec2 cellMin = visibleMin + glm::vec2(column, row) * kCheckerSize;
					drawList->AddRectFilled(ToImVec2(cellMin), ToImVec2(cellMin + kCheckerSize), kCheckerLightColor);
				}
			}

			drawList->AddImage(ToGuiTextureID(texture), ToImVec2(imageMin), ToImVec2(imageMin + imageSize));
			drawList->PopClipRect();
		}

		char zoomText[16];
		std::snprintf(zoomText, sizeof(zoomText), "%.0f%%", m_Scale * 100.0f);
		drawList->AddText(ToImVec2(canvasMin + 4.0f), IM_COL32(255, 255, 255, 160), zoomText);
	}

	TextureInspectorPanel::TextureKind TextureInspectorPanel::KindOf(const TextureStats& texture)
	{
		// The defaults hold the pool's first slots (see TexturePool's constructor)
		if (texture.Handle.Index < static_cast<uint32_t>(DefaultTexture::Count))
			return TextureKind::Default;
		return texture.IsRenderTarget ? TextureKind::RenderTarget : TextureKind::Texture;
	}

	const char* TextureInspectorPanel::KindName(TextureKind kind)
	{
		switch (kind)
		{
		case TextureKind::Default: return "Default";
		case TextureKind::Texture: return "Texture";
		case TextureKind::RenderTarget: return "Render target";
		}
		return "Unknown";
	}

}
