#include "cdlpch.h"
#include "GPUResourceInspectorPanel.h"

#include <imgui.h>

namespace Candle {

	namespace {

		float CalculateProgressBarPercentage(uint64_t currentValue, uint64_t maxValue)
		{
			if (maxValue == 0)
				return 0.0f; // Avoid division by zero
			return static_cast<float>(currentValue) / static_cast<float>(maxValue);
		}

		float BytesToMegabytes(uint64_t bytes)
		{
			return static_cast<float>(bytes) / (1024.0f * 1024.0f);
		}

	}

	GPUResourceInspectorPanel::GPUResourceInspectorPanel()
	{
		IsOpen = true;
	}

	GPUResourceInspectorPanel::~GPUResourceInspectorPanel()
	{

	}

	void GPUResourceInspectorPanel::OnUpdate(const DebugContext& context)
	{
	}

	void GPUResourceInspectorPanel::OnRender(const DebugContext& context)
	{
		ImGui::SetNextWindowSize(ImVec2(1000, 650), ImGuiCond_FirstUseEver);
		ImGui::Begin("GPU Resource Inspector");

		// Display TODO:
		// Top header area to show overal GPU resource usage stats (total memory used, total allocations, etc.)
		ImGui::Text("Total Uploads: %u", context.Resources.AllocatorStats.TotalUploads);
		ImGui::Text("Total Upload Bytes: %llu", context.Resources.AllocatorStats.TotalUploadBytes);
		ImGui::Text("Current VRAM Usage Bytes: %llu", context.Resources.AllocatorStats.CurrentVramUsageBytes);
		ImGui::Text("OS VRAM Budget Bytes: %llu", context.Resources.AllocatorStats.OsVramBudgetBytes);
		ImGui::Text("Device Max VRAM Bytes: %llu", context.Resources.AllocatorStats.DeviceMaxVramBytes);

		// 1. Capture the initial cursor position where the progress bars should start
		ImVec2 startCursorPos = ImGui::GetCursorPos();

		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.6f, 1.0f, 0.3f));

		// 2. Render the background / main progress bar (e.g., 70% complete)
		float budgetPercent = CalculateProgressBarPercentage(context.Resources.AllocatorStats.OsVramBudgetBytes, context.Resources.AllocatorStats.DeviceMaxVramBytes);
		ImGui::ProgressBar(budgetPercent, ImVec2(-1.0f, 0.0f), "");

		ImGui::PopStyleColor();

		// 3. Reset the cursor to the exact same starting point
		ImGui::SetCursorPos(startCursorPos);

		// 4. Push styles to differentiate the overlapping progress bar
		// Make the background transparent so the underlying bar shows through
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.6f, 1.0f, 0.8f));

		// 5. Render the overlapping progress bar (e.g., 40% complete)
		float currentUse = CalculateProgressBarPercentage(context.Resources.AllocatorStats.CurrentVramUsageBytes, context.Resources.AllocatorStats.DeviceMaxVramBytes);
		ImGui::ProgressBar(currentUse, ImVec2(-1.0f, 0.0f), "");

		// 6. Pop the styles to prevent bleeding into other UI elements
		ImGui::PopStyleColor(2);

		ImGui::SameLine();
		
		// Set color for the text based on usage percentage
		float usagePercentage = CalculateProgressBarPercentage(context.Resources.AllocatorStats.CurrentVramUsageBytes, context.Resources.AllocatorStats.OsVramBudgetBytes);
		//ImGui::PushStyleColor(ImGuiCol_Text, usagePercentage > 0.85f ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

		ImGui::Text("VRAM Usage: %.2f MB / %.2f MB", 
			BytesToMegabytes(context.Resources.AllocatorStats.CurrentVramUsageBytes), 
			BytesToMegabytes(context.Resources.AllocatorStats.DeviceMaxVramBytes));

		//ImGui::PopStyleColor();

		ImGui::Separator();
		
		// Expandable node to show heaps and their stats
		const ImGuiTreeNodeFlags treeNodeFlags =
			ImGuiTreeNodeFlags_DefaultOpen |
			ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_AllowOverlap |
			ImGuiTreeNodeFlags_FramePadding;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		if (ImGui::TreeNodeEx("Heaps", treeNodeFlags, "Heaps"))
		{
			if (ImGui::BeginTable("HeapTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				// Render Header row
				ImGui::TableSetupColumn("Heap Name");
				ImGui::TableSetupColumn("Size Bytes");
				ImGui::TableSetupColumn("Used Bytes");
				ImGui::TableSetupColumn("Unused Bytes");
				ImGui::TableHeadersRow();

				for (size_t i = 0; i < context.Resources.AllocatorStats.Heaps.size(); ++i)
				{
					const auto& heapStats = context.Resources.AllocatorStats.Heaps[i];
					if (heapStats.SizeBytes > 0)
					{
						const char* heapName = std::format("Heap {}", i).c_str();
						RenderHeapStats(heapName, heapStats);
					}
				}

				ImGui::EndTable();
			}
			ImGui::TreePop();
		}

		ImGui::PopStyleVar();

		// Expandable node to show pools and their stats
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		if (ImGui::TreeNodeEx("Pools", treeNodeFlags, "Pools"))
		{
			if (ImGui::BeginTable("PoolsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				// Render Header row
				ImGui::TableSetupColumn("Pool Name");
				ImGui::TableSetupColumn("Total Slots");
				ImGui::TableSetupColumn("Live Slots");
				ImGui::TableSetupColumn("Pending Slots");
				ImGui::TableSetupColumn("Free Slots");
				ImGui::TableHeadersRow();

				RenderPoolStats("Geometry", context.Resources.GeometryPoolStats);
				RenderPoolStats("Texture", context.Resources.TexturePoolStats);

				ImGui::EndTable();
			}
			ImGui::TreePop();
		}

		ImGui::PopStyleVar();

		ImGui::End();
	}

	void GPUResourceInspectorPanel::RenderHeapStats(const char* heapName, const GPUHeapStats& stats)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%s", heapName);
		ImGui::TableNextColumn();
		ImGui::Text("%llu", stats.SizeBytes);
		ImGui::TableNextColumn();
		ImGui::Text("%llu", stats.UsedBytes);
		ImGui::TableNextColumn();
		ImGui::Text("%llu", stats.UnusedBytes);

		// TOOD: Have progress bar for heap usage, similar to the one at the top of the panel
	}

	void GPUResourceInspectorPanel::RenderPoolStats(const char* poolName, const GPUResourcePoolStats& stats)
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text("%s", poolName);
		ImGui::TableNextColumn();
		ImGui::Text("%u", stats.TotalSlots);
		ImGui::TableNextColumn();
		ImGui::Text("%u", stats.LiveSlots);
		ImGui::TableNextColumn();
		ImGui::Text("%u", stats.PendingSlots);
		ImGui::TableNextColumn();
		ImGui::Text("%u", stats.FreeSlots);

		// TODO: Have block map of pool usage with lightened up sub-blocks being lit up for each live slot, and darkened for free slots. Pending slots can be a different color.
	}

}