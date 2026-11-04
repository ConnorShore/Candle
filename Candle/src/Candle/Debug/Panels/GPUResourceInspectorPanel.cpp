#include "cdlpch.h"
#include "GPUResourceInspectorPanel.h"

namespace Candle {

	void GPUResourceInspectorPanel::OnUpdate(const DebugContext& context)
	{
		static int frameCount{ 0 };
		if (++frameCount % 600 == 0)
		{
			// Log every stat from context.Resources
			CDL_CORE_INFO(LogChannel::Debug, "GPU Resource Stats:");
			CDL_CORE_INFO(LogChannel::Debug, "  Geometry Pool: Total Slots: {}, Live Slots: {}, Pending Slots: {}, Free Slots: {}, Size Bytes: {}",
				context.Resources.GeometryPoolStats.TotalSlots,
				context.Resources.GeometryPoolStats.LiveSlots,
				context.Resources.GeometryPoolStats.PendingSlots,
				context.Resources.GeometryPoolStats.FreeSlots,
				context.Resources.GeometryPoolStats.SizeBytes);
			CDL_CORE_INFO(LogChannel::Debug, "  Texture Pool: Total Slots: {}, Live Slots: {}, Pending Slots: {}, Free Slots: {}, Size Bytes: {}",
				context.Resources.TexturePoolStats.TotalSlots,
				context.Resources.TexturePoolStats.LiveSlots,
				context.Resources.TexturePoolStats.PendingSlots,
				context.Resources.TexturePoolStats.FreeSlots,
				context.Resources.TexturePoolStats.SizeBytes);
			CDL_CORE_INFO(LogChannel::Debug, "  GPU Allocator: Total Uploads: {}, Total Upload Bytes: {}",
				context.Resources.AllocatorStats.TotalUploads,
				context.Resources.AllocatorStats.TotalUploadBytes);
			for (const auto& heapStats : context.Resources.AllocatorStats.Heaps)
			{
				if (heapStats.SizeBytes > 0)
				{
					CDL_CORE_INFO(LogChannel::Debug, "    Heap Type: {}, Size Bytes: {}, Used Bytes: {}, Unused Bytes: {}",
						heapStats.HeapType,
						heapStats.SizeBytes,
						heapStats.UsedBytes,
						heapStats.UnusedBytes);
				}
			}
		}
	}

	void GPUResourceInspectorPanel::OnRender(const DebugContext& context)
	{

	}

}