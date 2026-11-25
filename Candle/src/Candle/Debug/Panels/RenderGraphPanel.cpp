#include "cdlpch.h"
#include "RenderGraphPanel.h"

#include "Candle/Render/FrameGraph/FrameGraphCapture.h"
#include "Candle/Render/GUI/GuiTextureID.h"

#include <imgui.h>
#include <imgui_internal.h>	// TableAngledHeadersRowEx, which takes a colour per pass

#include <algorithm>
#include <format>
#include <numeric>
#include <string>
#include <tuple>

namespace Candle {

	namespace {

		constexpr float kDetailsWidthFraction = 0.32f;	// The details pane's share of the window
		constexpr float kCellWidthScale = 1.8f;			// Of the frame height
		constexpr float kCellInset = 2.0f;
		constexpr float kMinPreviewSize = 32.0f;

		constexpr ImU32 kLivePassColor = IM_COL32(215, 120, 20, 255);
		constexpr ImU32 kCulledPassColor = IM_COL32(75, 75, 75, 255);
		constexpr ImU32 kPreviewPassColor = IM_COL32(120, 90, 170, 255);
		constexpr ImU32 kImportedColor = IM_COL32(35, 60, 140, 255);
		constexpr ImU32 kTransientColor = IM_COL32(25, 110, 160, 255);
		constexpr ImU32 kUnusedColor = IM_COL32(60, 60, 60, 255);
		constexpr ImU32 kWriteColor = IM_COL32(210, 70, 55, 255);
		constexpr ImU32 kReadColor = IM_COL32(60, 170, 90, 255);
		constexpr ImU32 kLifetimeColor = IM_COL32(255, 255, 255, 20);
		constexpr ImU32 kOverlapColor = IM_COL32(230, 200, 80, 60);
		constexpr ImU32 kPausedColor = IM_COL32(230, 200, 80, 255);

		ImU32 Faded(ImU32 color)
		{
			return (color & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 90);
		}

		const char* UsageName(FGUsage usage)
		{
			switch (usage)
			{
			case FGUsage::TransferSrc: return "TransferSrc";
			case FGUsage::TransferDst: return "TransferDst";
			case FGUsage::ColorAttachment: return "ColorAttachment";
			case FGUsage::DepthAttachment: return "DepthAttachment";
			case FGUsage::SampledVertex: return "SampledVertex";
			case FGUsage::SampledFragment: return "SampledFragment";
			case FGUsage::SampledCompute: return "SampledCompute";
			case FGUsage::StorageReadVertex: return "StorageReadVertex";
			case FGUsage::StorageReadFragment: return "StorageReadFragment";
			case FGUsage::StorageReadCompute: return "StorageReadCompute";
			case FGUsage::StorageWriteVertex: return "StorageWriteVertex";
			case FGUsage::StorageWriteFragment: return "StorageWriteFragment";
			case FGUsage::StorageWriteCompute: return "StorageWriteCompute";
			}
			return "Unknown";
		}

		const char* LoadName(FGLoad load)
		{
			switch (load)
			{
			case FGLoad::Load: return "Load";
			case FGLoad::Clear: return "Clear";
			case FGLoad::DontCare: return "DontCare";
			}
			return "Unknown";
		}

		bool IsAttachment(FGUsage usage)
		{
			return usage == FGUsage::ColorAttachment || usage == FGUsage::DepthAttachment;
		}

		ImU32 PassColor(const FrameGraphSnapshot::Pass& pass)
		{
			if (!pass.Live)
				return kCulledPassColor;
			return pass.Name == kFrameGraphPreviewPassName ? kPreviewPassColor : kLivePassColor;
		}

		void DrawLegendEntry(ImU32 color, const char* label)
		{
			const float size = ImGui::GetTextLineHeight();
			const ImVec2 min = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddRectFilled(min, { min.x + size, min.y + size }, color, 2.0f);
			ImGui::Dummy({ size, size });
			ImGui::SameLine();
			ImGui::TextUnformatted(label);
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 3.0f);
		}

		void DrawStateRow(const char* label, const ImageState& state)
		{
			ImGui::Text("%s: %s", label, vk::to_string(state.Layout).c_str());
			ImGui::TextDisabled("  %s", vk::to_string(state.Stage).c_str());
			ImGui::TextDisabled("  %s", vk::to_string(state.Access).c_str());
		}

	}

	void RenderGraphPanel::OnRender(const DebugContext& context)
	{
		TakeLatestSnapshot();

		ImGui::SetNextWindowSize({ 1100.0f, 600.0f }, ImGuiCond_FirstUseEver);
		if (ImGui::Begin(GetName(), &IsOpen))
		{
			if (!m_HasSnapshot)
			{
				ImGui::TextDisabled("Waiting for the render thread's first snapshot");
			}
			else
			{
				DrawToolbar();
				DrawLegend();
				ImGui::Separator();

				ImGui::BeginChild("##MatrixArea", { ImGui::GetContentRegionAvail().x * (1.0f - kDetailsWidthFraction), 0.0f });
				DrawMatrix();
				ImGui::EndChild();

				ImGui::SameLine();
				ImGui::BeginChild("##Details", { 0.0f, 0.0f }, ImGuiChildFlags_Borders);
				DrawDetails();
				ImGui::EndChild();
			}
		}
		ImGui::End();

		// Last, so the request carries this frame's selection
		RequestCapture();
	}

	void RenderGraphPanel::TakeLatestSnapshot()
	{
		const FrameGraphSnapshot* latest = m_Capture.GetSnapshot();
		if (!latest || (m_HasSnapshot && (m_Paused || latest->FrameIndex == m_Snapshot.FrameIndex)))
			return;

		m_Snapshot = *latest;
		m_HasSnapshot = true;
		RebuildCells();
	}

	void RenderGraphPanel::RebuildCells()
	{
		const uint32_t passCount = static_cast<uint32_t>(m_Snapshot.Passes.size());
		const uint32_t textureCount = static_cast<uint32_t>(m_Snapshot.Textures.size());
		m_Lifetimes.assign(textureCount, Lifetime{});
		m_CellAccesses.assign(size_t{ textureCount } * passCount, FrameGraphSnapshot::kNone);

		for (uint32_t pass = 0; pass < passCount; ++pass)
		{
			const FrameGraphSnapshot::Pass& info = m_Snapshot.Passes[pass];
			for (uint32_t i = info.AccessStart; i < info.AccessStart + info.AccessCount; ++i)
			{
				const FGAccessEntry& access = m_Snapshot.Accesses[i];
				if (access.Type != FGResourceType::Texture || access.ResourceIndex >= textureCount)
					continue;

				// A pass that both reads and writes a texture shows the write
				uint32_t& cell = m_CellAccesses[size_t{ access.ResourceIndex } * passCount + pass];
				if (cell == FrameGraphSnapshot::kNone || access.Write)
					cell = i;

				if (info.Live)
				{
					Lifetime& lifetime = m_Lifetimes[access.ResourceIndex];
					lifetime.First = std::min(lifetime.First, pass);
					lifetime.Last = std::max(lifetime.Last, pass);
				}
			}
		}
	}

	void RenderGraphPanel::RequestCapture()
	{
		FrameGraphCaptureRequest request{ .Snapshot = true, .PreviewRange = m_PreviewRange };

		// Named from the snapshot on screen, so while paused it's the texture the user is looking at. Paused, the preview holds
		// still once it shows the selection, and is only drawn again when the selection or range changes.
		if (const std::optional<uint32_t> texture = FindTextureIndex(m_SelectedTexture); texture && IsPreviewable(*texture))
		{
			const std::optional<FrameGraphPreview>& preview = m_Capture.GetPreview();
			const bool showsSelection = preview && preview->Texture == m_SelectedTexture;
			request.PreviewTexture = m_SelectedTexture;
			request.CapturePreview = !m_Paused || !showsSelection || m_RecapturePreview;
		}

		m_RecapturePreview = false;
		m_Capture.Request(std::move(request));
	}

	void RenderGraphPanel::DrawToolbar()
	{
		ImGui::Checkbox("Pause", &m_Paused);
		ImGui::SetItemTooltip("Hold this frame's graph to inspect it. The preview holds the last frame it drew, which may be a frame or two later.");
		ImGui::SameLine();
		ImGui::Checkbox("Show culled passes", &m_ShowCulled);
		ImGui::SameLine();
		ImGui::Checkbox("Highlight overlaps", &m_HighlightOverlaps);
		ImGui::SetItemTooltip("Shade each texture's lifetime. Hover a texture to see which others are alive at the same time, so can't share its memory.");
		ImGui::SameLine();
		ImGui::Checkbox("Sort by lifetime", &m_SortByLifetime);

		const auto livePasses = std::ranges::count_if(m_Snapshot.Passes, &FrameGraphSnapshot::Pass::Live);
		const auto importedTextures = std::ranges::count_if(m_Snapshot.Textures, &FrameGraphSnapshot::Texture::Imported);
		ImGui::Text("Frame %llu  |  Passes: %zu (%lld live, %lld culled)  |  Textures: %zu (%lld imported, %lld transient)  |  Barriers: %zu  |  Output: %u x %u",
			static_cast<unsigned long long>(m_Snapshot.FrameIndex),
			m_Snapshot.Passes.size(), static_cast<long long>(livePasses), static_cast<long long>(m_Snapshot.Passes.size() - livePasses),
			m_Snapshot.Textures.size(), static_cast<long long>(importedTextures), static_cast<long long>(m_Snapshot.Textures.size() - importedTextures),
			m_Snapshot.Barriers.size() + m_Snapshot.FinalBarriers.size(), m_Snapshot.OutputExtent.x, m_Snapshot.OutputExtent.y);

		if (m_Paused)
		{
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 4.0f);
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kPausedColor), "PAUSED");
		}
	}

	void RenderGraphPanel::DrawLegend()
	{
		DrawLegendEntry(kLivePassColor, "Live pass");
		DrawLegendEntry(kCulledPassColor, "Culled pass");
		DrawLegendEntry(kPreviewPassColor, "Preview pass");
		DrawLegendEntry(kImportedColor, "Imported");
		DrawLegendEntry(kTransientColor, "Transient");
		DrawLegendEntry(kUnusedColor, "Unused");
		DrawLegendEntry(kWriteColor, "Write");
		DrawLegendEntry(kReadColor, "Read");
		ImGui::NewLine();
	}

	void RenderGraphPanel::DrawMatrix()
	{
		m_Columns.clear();
		for (uint32_t pass = 0; pass < m_Snapshot.Passes.size(); ++pass)
		{
			if (m_ShowCulled || m_Snapshot.Passes[pass].Live)
				m_Columns.push_back(pass);
		}

		m_Rows.resize(m_Snapshot.Textures.size());
		std::iota(m_Rows.begin(), m_Rows.end(), 0u);
		if (m_SortByLifetime)
		{
			// Unused textures have First at kNone, so they sort last
			std::ranges::stable_sort(m_Rows, [this](uint32_t a, uint32_t b) {
				return std::tie(m_Lifetimes[a].First, m_Lifetimes[a].Last) < std::tie(m_Lifetimes[b].First, m_Lifetimes[b].Last);
			});
		}

		constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuter
			| ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_HighlightHoveredColumn | ImGuiTableFlags_NoSavedSettings;
		if (!ImGui::BeginTable("##Matrix", static_cast<int>(m_Columns.size()) + 1, kTableFlags))
			return;

		const float cellWidth = ImGui::GetFrameHeight() * kCellWidthScale;
		ImGui::TableSetupScrollFreeze(1, 2);
		ImGui::TableSetupColumn("Texture", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide);
		for (const uint32_t pass : m_Columns)
		{
			const FrameGraphSnapshot::Pass& info = m_Snapshot.Passes[pass];
			const std::string label = info.Live ? info.Name : std::format("{} (culled)", info.Name);
			ImGui::TableSetupColumn(label.c_str(), ImGuiTableColumnFlags_AngledHeader | ImGuiTableColumnFlags_WidthFixed, cellWidth);
		}

		const int hoveredColumn = ImGui::TableGetHoveredColumn();
		std::vector<ImGuiTableHeaderData> headers;
		headers.reserve(m_Columns.size());
		for (size_t i = 0; i < m_Columns.size(); ++i)
		{
			const int column = static_cast<int>(i) + 1;
			const FrameGraphSnapshot::Pass& info = m_Snapshot.Passes[m_Columns[i]];
			ImU32 highlight = 0;
			if (column == hoveredColumn)
				highlight = ImGui::GetColorU32(ImGuiCol_HeaderHovered);
			else if (info.Name == m_SelectedPass)
				highlight = ImGui::GetColorU32(ImGuiCol_HeaderActive);

			headers.push_back({ .Index = static_cast<ImGuiTableColumnIdx>(column), .TextColor = IM_COL32_WHITE, .BgColor0 = PassColor(info), .BgColor1 = highlight });
		}
		ImGui::TableAngledHeadersRowEx(ImGui::GetID("##AngledHeaders"), ImGui::GetStyle().TableAngledHeadersAngle, 0.0f, headers.data(), static_cast<int>(headers.size()));

		// The pass numbers under the angled names, which select a pass when clicked
		ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
		ImGui::TableSetColumnIndex(0);
		ImGui::TableHeader("Texture");
		for (size_t i = 0; i < m_Columns.size(); ++i)
		{
			if (!ImGui::TableSetColumnIndex(static_cast<int>(i) + 1))
				continue;

			const uint32_t pass = m_Columns[i];
			ImGui::PushID(static_cast<int>(pass));
			ImGui::TableHeader(std::to_string(pass).c_str());
			if (ImGui::IsItemClicked())
				SelectPass(pass);
			ImGui::SetItemTooltip("%s", m_Snapshot.Passes[pass].Name.c_str());
			ImGui::PopID();
		}

		std::optional<uint32_t> hoveredTexture;
		const Lifetime* hoveredLifetime = m_HighlightOverlaps && m_HoveredTexture && *m_HoveredTexture < m_Lifetimes.size() ? &m_Lifetimes[*m_HoveredTexture] : nullptr;
		for (const uint32_t texture : m_Rows)
		{
			const FrameGraphSnapshot::Texture& info = m_Snapshot.Textures[texture];
			const Lifetime& lifetime = m_Lifetimes[texture];
			ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFrameHeight());
			ImGui::PushID(static_cast<int>(texture));

			ImGui::TableSetColumnIndex(0);
			ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, !lifetime.IsUsed() ? kUnusedColor : info.Imported ? kImportedColor : kTransientColor);
			ImGui::AlignTextToFramePadding();
			if (ImGui::Selectable(info.Name.c_str(), info.Name == m_SelectedTexture))
				SelectTexture(texture);
			if (ImGui::IsItemHovered())
				hoveredTexture = texture;

			const bool overlapsHovered = hoveredLifetime && lifetime.Overlaps(*hoveredLifetime);
			for (size_t i = 0; i < m_Columns.size(); ++i)
			{
				if (!ImGui::TableSetColumnIndex(static_cast<int>(i) + 1))
					continue;

				const uint32_t pass = m_Columns[i];
				if (m_HighlightOverlaps && lifetime.Contains(pass))
					ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, overlapsHovered ? kOverlapColor : kLifetimeColor);

				if (DrawCell(texture, pass))
					hoveredTexture = texture;
			}

			ImGui::PopID();
		}

		m_HoveredTexture = hoveredTexture;
		ImGui::EndTable();
	}

	bool RenderGraphPanel::DrawCell(uint32_t texture, uint32_t pass)
	{
		const uint32_t accessIndex = CellAccess(texture, pass);
		if (accessIndex == FrameGraphSnapshot::kNone)
			return false;

		const FGAccessEntry& access = m_Snapshot.Accesses[accessIndex];
		const ImVec2 min = ImGui::GetCursorScreenPos();
		const ImVec2 size(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight());

		ImGui::PushID(static_cast<int>(pass));
		ImGui::InvisibleButton("##Cell", size);
		const bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked())
			SelectPass(pass);
		if (ImGui::BeginItemTooltip())
		{
			DrawAccessTooltip(texture, pass, access);
			ImGui::EndTooltip();
		}
		ImGui::PopID();

		const ImU32 color = access.Write ? kWriteColor : kReadColor;
		const ImVec2 rectMin(min.x + kCellInset, min.y + kCellInset);
		const ImVec2 rectMax(min.x + size.x - kCellInset, min.y + size.y - kCellInset);
		const char* label = access.Write ? "w" : "r";
		const ImVec2 labelSize = ImGui::CalcTextSize(label);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(rectMin, rectMax, m_Snapshot.Passes[pass].Live ? color : Faded(color), 2.0f);
		drawList->AddText({ (rectMin.x + rectMax.x - labelSize.x) * 0.5f, (rectMin.y + rectMax.y - labelSize.y) * 0.5f }, IM_COL32_WHITE, label);
		return hovered;
	}

	void RenderGraphPanel::DrawAccessTooltip(uint32_t texture, uint32_t pass, const FGAccessEntry& access)
	{
		const FrameGraphSnapshot::Pass& passInfo = m_Snapshot.Passes[pass];
		ImGui::Text("%s %s", passInfo.Name.c_str(), access.Write ? "writes" : "reads");
		ImGui::Text("%s, version %u", m_Snapshot.Textures[texture].Name.c_str(), access.Version);
		ImGui::TextDisabled("as %s", UsageName(access.Usage));
		if (IsAttachment(access.Usage))
			ImGui::TextDisabled("load %s", LoadName(access.LoadOp));
		if (access.SrcPassIndex < m_Snapshot.Passes.size())
			ImGui::TextDisabled("after %s", m_Snapshot.Passes[access.SrcPassIndex].Name.c_str());

		if (!passInfo.Live)
		{
			ImGui::TextDisabled("Culled, so no barrier");
			return;
		}

		ImGui::Separator();
		if (const FGBarrier* barrier = FindBarrier(pass, texture))
		{
			DrawStateRow("From", barrier->FromState);
			DrawStateRow("To", barrier->ToState);
		}
		else
		{
			ImGui::TextDisabled("No barrier: already where this pass needs it");
		}
	}

	void RenderGraphPanel::DrawDetails()
	{
		if (!m_SelectedPass.empty())
		{
			if (const std::optional<uint32_t> pass = FindPassIndex(m_SelectedPass))
				DrawPassDetails(*pass);
			else
				ImGui::TextDisabled("%s isn't in frame %llu", m_SelectedPass.c_str(), static_cast<unsigned long long>(m_Snapshot.FrameIndex));
			return;
		}

		if (!m_SelectedTexture.empty())
		{
			if (const std::optional<uint32_t> texture = FindTextureIndex(m_SelectedTexture))
				DrawTextureDetails(*texture);
			else
				ImGui::TextDisabled("%s isn't in frame %llu", m_SelectedTexture.c_str(), static_cast<unsigned long long>(m_Snapshot.FrameIndex));
			return;
		}

		ImGui::BeginDisabled();
		ImGui::TextWrapped("Click a texture's name to inspect and preview it, or a cell or pass number to inspect the pass");
		ImGui::EndDisabled();
	}

	void RenderGraphPanel::DrawPassDetails(uint32_t pass)
	{
		const FrameGraphSnapshot::Pass& info = m_Snapshot.Passes[pass];
		ImGui::PushStyleColor(ImGuiCol_Text, PassColor(info));
		ImGui::SeparatorText(info.Name.c_str());
		ImGui::PopStyleColor();

		ImGui::Text("Pass %u, %s, %s", pass, info.Compute ? "compute" : "raster", info.Live ? "live" : "culled");
		ImGui::TextDisabled("%s", LiveReason(pass).c_str());

		ImGui::SeparatorText("Accesses");
		constexpr ImGuiTableFlags kListFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_NoSavedSettings;
		if (ImGui::BeginTable("##Accesses", 4, kListFlags))
		{
			ImGui::TableSetupColumn("Texture");
			ImGui::TableSetupColumn("Access");
			ImGui::TableSetupColumn("Usage");
			ImGui::TableSetupColumn("Version");
			ImGui::TableHeadersRow();

			for (uint32_t i = info.AccessStart; i < info.AccessStart + info.AccessCount; ++i)
			{
				const FGAccessEntry& access = m_Snapshot.Accesses[i];
				if (access.ResourceIndex >= m_Snapshot.Textures.size())
					continue;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::PushID(static_cast<int>(i));
				if (ImGui::Selectable(m_Snapshot.Textures[access.ResourceIndex].Name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
					SelectTexture(access.ResourceIndex);
				ImGui::PopID();
				ImGui::TableNextColumn();
				ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(access.Write ? kWriteColor : kReadColor), "%s", access.Write ? "Write" : "Read");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(UsageName(access.Usage));
				if (IsAttachment(access.Usage))
				{
					ImGui::SameLine();
					ImGui::TextDisabled("(%s)", LoadName(access.LoadOp));
				}
				ImGui::TableNextColumn();
				ImGui::Text("%u", access.Version);
			}
			ImGui::EndTable();
		}

		if (!info.Live)
			return;

		ImGui::SeparatorText("Barriers");
		if (info.BarrierCount == 0)
		{
			ImGui::TextDisabled("None: every texture was already where this pass needs it");
			return;
		}

		if (ImGui::BeginTable("##Barriers", 2, kListFlags))
		{
			ImGui::TableSetupColumn("Texture");
			ImGui::TableSetupColumn("Layout");
			ImGui::TableHeadersRow();

			for (uint32_t i = info.BarrierStart; i < info.BarrierStart + info.BarrierCount; ++i)
			{
				const FGBarrier& barrier = m_Snapshot.Barriers[i];
				if (barrier.Texture.Index >= m_Snapshot.Textures.size())
					continue;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(m_Snapshot.Textures[barrier.Texture.Index].Name.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%s -> %s", vk::to_string(barrier.FromState.Layout).c_str(), vk::to_string(barrier.ToState.Layout).c_str());
				if (ImGui::BeginItemTooltip())
				{
					DrawStateRow("From", barrier.FromState);
					DrawStateRow("To", barrier.ToState);
					ImGui::EndTooltip();
				}
			}
			ImGui::EndTable();
		}
	}

	void RenderGraphPanel::DrawTextureDetails(uint32_t texture)
	{
		const FrameGraphSnapshot::Texture& info = m_Snapshot.Textures[texture];
		const Lifetime& lifetime = m_Lifetimes[texture];
		ImGui::SeparatorText(info.Name.c_str());

		const char* kind = info.Imported ? "Imported" : "Transient";
		ImGui::Text("%s%s", kind, lifetime.IsUsed() ? "" : ", unused this frame");
		ImGui::Text("%u x %u, %s", info.Extent.x, info.Extent.y, vk::to_string(info.Format).c_str());
		ImGui::TextWrapped("Usage: %s", info.Usage ? vk::to_string(info.Usage).c_str() : "none");
		ImGui::Text("Written %u time%s", info.LatestVersion, info.LatestVersion == 1 ? "" : "s");
		if (lifetime.IsUsed())
			ImGui::Text("Alive from %s to %s", m_Snapshot.Passes[lifetime.First].Name.c_str(), m_Snapshot.Passes[lifetime.Last].Name.c_str());
		if (!info.Imported && lifetime.IsUsed())
			ImGui::TextDisabled("Cache slot %u, bindless %u", info.CacheIndex, info.BindlessIndex);

		ImGui::SeparatorText("Passes");
		for (uint32_t pass = 0; pass < m_Snapshot.Passes.size(); ++pass)
		{
			const uint32_t accessIndex = CellAccess(texture, pass);
			if (accessIndex == FrameGraphSnapshot::kNone)
				continue;

			const FGAccessEntry& access = m_Snapshot.Accesses[accessIndex];
			ImGui::PushID(static_cast<int>(pass));
			if (ImGui::Selectable(m_Snapshot.Passes[pass].Name.c_str()))
				SelectPass(pass);
			ImGui::PopID();
			ImGui::SameLine();
			ImGui::TextDisabled("%s %s", access.Write ? "writes" : "reads", UsageName(access.Usage));
		}

		if (!info.Imported)
		{
			ImGui::SeparatorText("Preview");
			DrawPreview(texture);
		}
	}

	void RenderGraphPanel::DrawPreview(uint32_t texture)
	{
		const FrameGraphSnapshot::Texture& info = m_Snapshot.Textures[texture];
		if (!m_Lifetimes[texture].IsUsed())
		{
			ImGui::TextDisabled("Culled, so it wasn't allocated this frame");
			return;
		}
		if (!IsPreviewable(texture))
		{
			ImGui::TextDisabled("No preview for %s", vk::to_string(info.Format).c_str());
			return;
		}

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
		if (ImGui::DragFloatRange2("Range", &m_PreviewRange.x, &m_PreviewRange.y, 0.001f, 0.0f, 1.0f, "%.3f", nullptr, ImGuiSliderFlags_AlwaysClamp))
			m_RecapturePreview = true;
		ImGui::SetItemTooltip("Spread from black to white. Depth crowds near 1, so narrow this to about 0.99..1 to see the scene in it.");
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			m_PreviewRange = { 0.0f, 1.0f };
			m_RecapturePreview = true;
		}

		const std::optional<FrameGraphPreview>& preview = m_Capture.GetPreview();
		if (!preview || preview->Texture != info.Name)
		{
			ImGui::TextDisabled("Capturing...");
			return;
		}

		if (preview->FrameIndex == m_Snapshot.FrameIndex)
			ImGui::TextDisabled("Frame %llu", static_cast<unsigned long long>(preview->FrameIndex));
		else
			ImGui::TextDisabled("From frame %llu, not the graph's frame %llu", static_cast<unsigned long long>(preview->FrameIndex), static_cast<unsigned long long>(m_Snapshot.FrameIndex));

		const glm::vec2 extent(glm::max(info.Extent, glm::uvec2(1)));
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const float scale = std::max(std::min(available.x / extent.x, available.y / extent.y), kMinPreviewSize / std::max(extent.x, extent.y));
		ImGui::Image(ToGuiTextureID(preview->Target), { extent.x * scale, extent.y * scale });
	}

	void RenderGraphPanel::SelectPass(uint32_t pass)
	{
		m_SelectedPass = m_Snapshot.Passes[pass].Name;
		m_SelectedTexture.clear();
	}

	void RenderGraphPanel::SelectTexture(uint32_t texture)
	{
		const std::string& name = m_Snapshot.Textures[texture].Name;
		if (name != m_SelectedTexture)
			m_PreviewRange = { 0.0f, 1.0f };

		m_SelectedTexture = name;
		m_SelectedPass.clear();
	}

	std::optional<uint32_t> RenderGraphPanel::FindPassIndex(const std::string& name) const
	{
		const auto it = std::ranges::find(m_Snapshot.Passes, name, &FrameGraphSnapshot::Pass::Name);
		if (name.empty() || it == m_Snapshot.Passes.end())
			return std::nullopt;
		return static_cast<uint32_t>(std::distance(m_Snapshot.Passes.begin(), it));
	}

	std::optional<uint32_t> RenderGraphPanel::FindTextureIndex(const std::string& name) const
	{
		const auto it = std::ranges::find(m_Snapshot.Textures, name, &FrameGraphSnapshot::Texture::Name);
		if (name.empty() || it == m_Snapshot.Textures.end())
			return std::nullopt;
		return static_cast<uint32_t>(std::distance(m_Snapshot.Textures.begin(), it));
	}

	uint32_t RenderGraphPanel::CellAccess(uint32_t texture, uint32_t pass) const
	{
		return m_CellAccesses[size_t{ texture } * m_Snapshot.Passes.size() + pass];
	}

	const FGBarrier* RenderGraphPanel::FindBarrier(uint32_t pass, uint32_t texture) const
	{
		const FrameGraphSnapshot::Pass& info = m_Snapshot.Passes[pass];
		for (uint32_t i = info.BarrierStart; i < info.BarrierStart + info.BarrierCount; ++i)
		{
			if (m_Snapshot.Barriers[i].Texture.Index == texture)
				return &m_Snapshot.Barriers[i];
		}
		return nullptr;
	}

	std::string RenderGraphPanel::LiveReason(uint32_t pass) const
	{
		const FrameGraphSnapshot::Pass& info = m_Snapshot.Passes[pass];
		if (!info.Live)
			return "Culled: no live pass reads what it writes, and it writes nothing imported";
		if (info.NeverCull)
			return "Marked NeverCull";

		for (uint32_t i = info.AccessStart; i < info.AccessStart + info.AccessCount; ++i)
		{
			const FGAccessEntry& access = m_Snapshot.Accesses[i];
			if (access.Write && access.ResourceIndex < m_Snapshot.Textures.size() && m_Snapshot.Textures[access.ResourceIndex].Imported)
				return std::format("Writes {}, which outlives the frame", m_Snapshot.Textures[access.ResourceIndex].Name);
		}

		for (const FrameGraphSnapshot::Pass& reader : m_Snapshot.Passes)
		{
			if (!reader.Live)
				continue;
			for (uint32_t i = reader.AccessStart; i < reader.AccessStart + reader.AccessCount; ++i)
			{
				if (m_Snapshot.Accesses[i].SrcPassIndex == pass)
					return std::format("Feeds {}", reader.Name);
			}
		}
		return "Live";
	}

	bool RenderGraphPanel::IsPreviewable(uint32_t texture) const
	{
		const FrameGraphSnapshot::Texture& info = m_Snapshot.Textures[texture];
		return !info.Imported && info.LatestVersion > 0 && m_Lifetimes[texture].IsUsed() && IsPreviewableFormat(info.Format);
	}

}
