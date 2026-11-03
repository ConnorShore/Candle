#include "cdlpch.h"
#include "TextureInspectorPanel.h"
#include "Candle/Render/Resources/TexturePool.h"
#include "Candle/Render/GUI/GuiTextureID.h"

#include <imgui.h>

namespace Candle {

	void TextureInspectorPanel::OnUpdate(const DebugContext& context)
	{

	}

	void TextureInspectorPanel::OnRender(const DebugContext& context)
	{
		ImGui::Begin("Texture Inspector", &IsOpen);

		const TexturePoolStats stats = m_TexturePool.CaptureStats();
		if (ImGui::BeginTable("TextureTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			for (const auto& textureInfo : stats.ActiveTextures)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::PushID(textureInfo.Handle.Index);

				bool isSelected = (m_SelectedTexture == textureInfo.Handle);
				if (ImGui::Selectable(std::to_string(textureInfo.Handle.Index).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
					m_SelectedTexture = textureInfo.Handle;

				ImGui::TableNextColumn();
				ImGui::Text("%s", textureInfo.DebugName.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%u x %u", textureInfo.Extent.x, textureInfo.Extent.y);
				ImGui::TableNextColumn();
				ImGui::Text("%s", vk::to_string(textureInfo.Format).c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%u mip levels", textureInfo.MipLevels);
				ImGui::TableNextColumn();
				ImGui::Text("%u bytes", textureInfo.SizeBytes);


				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		ImGui::Separator();

		// A selection outlives its texture, and a released handle must never reach the packet
		if (!m_TexturePool.IsAlive(m_SelectedTexture))
			m_SelectedTexture = {};

		if (m_SelectedTexture.IsValid())
		{
			ImGui::Image(ToGuiTextureID(m_SelectedTexture), ImVec2(256, 256));
		}

		ImGui::End();
	}

}