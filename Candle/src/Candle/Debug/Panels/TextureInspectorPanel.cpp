#include "cdlpch.h"
#include "TextureInspectorPanel.h"

#include <imgui.h>

namespace Candle {

	namespace {

		std::string FormatToString(TextureFormat format)
		{
			switch (format)
			{
			case TextureFormat::Rgba8: return "RGBA8";
			case TextureFormat::Rg8: return "RG8";
			case TextureFormat::R8: return "R8";
			default: return "Unknown";
			}
		}

	}

	void TextureInspectorPanel::OnUpdate(const DebugContext& context)
	{

	}

	void TextureInspectorPanel::OnRender(const DebugContext& context)
	{
		ImGui::Begin("Texture Inspector", &IsOpen);

		if (ImGui::BeginTable("TextureTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			for (const auto& textureInfo : context.Render.TextureStats.ActiveTextures)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%s", textureInfo.Index == std::numeric_limits<uint32_t>::max() ? "Invalid" : std::to_string(textureInfo.Index).c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%u x %u", textureInfo.Extent.x, textureInfo.Extent.y);
				ImGui::TableNextColumn();
				ImGui::Text("%s", FormatToString(textureInfo.Format).c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%u mip levels", textureInfo.MipLevels);
				ImGui::TableNextColumn();
				ImGui::Text("%u bytes", textureInfo.SizeBytes);
			}

			ImGui::EndTable();
		}

		ImGui::End();
	}

}