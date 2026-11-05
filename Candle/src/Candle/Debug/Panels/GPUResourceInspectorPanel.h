#pragma once

#include "Candle/Debug/DebugPanel.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace Candle {

	class GPUResourceInspectorPanel : public DebugPanel
	{
	public:
		GPUResourceInspectorPanel() = default;
		~GPUResourceInspectorPanel() override = default;

		virtual void OnUpdate(const DebugContext& context) override;
		virtual void OnRender(const DebugContext& context) override;

		inline virtual const char* GetName() const override { return "GPU Resource Inspector"; }
		inline virtual DebugPanelCategory GetCategory() const override { return DebugPanelCategory::Memory; }
		inline virtual DebugPanelCommand GetCommand() override
		{
			return DebugPanelCommand{
				.Name{ "GPUResources" },
				.Description{ "GPU Resource Inspector Panel" },
				.Usage{ "GPUResources" }
			};
		}

	private:
		enum class Metric { VramUsage, LiveMeshes, LiveTextures, PendingSlots, UploadRate, WorstStall, Count };

		static constexpr size_t kHistorySize = 300;	// Five minutes at one sample a second: long enough to see a slow leak

		using ResourceSample = std::array<float, static_cast<size_t>(Metric::Count)>;

		struct PoolActivity
		{
			uint64_t LastReleases{ 0 };
			uint32_t FramesSinceRelease{ 0 };
			float ReleasesPerSecond{ 0.0f };
		};

		void TrackPool(PoolActivity& activity, const GPUResourcePoolStats& stats);
		void PushSample(const GPUResourceStats& stats);	// Publishes the rates over the sample that just ended

		void RenderToolbar(const GPUResourceStats& stats);
		void RenderMemorySection(const GPUAllocatorStats& stats);
		void RenderPoolsSection(const GPUResourceStats& stats);
		void RenderActivitySection(const GPUResourceStats& stats);
		void RenderHistorySection();

		void RenderHeapStats(uint32_t index, const GPUHeapStats& stats);
		void RenderPoolStats(const char* poolName, const GPUResourcePoolStats& stats, const GPUResourcePoolStats* baseline, const PoolActivity& activity);
		void RenderOtherMemory(const GPUResourceStats& stats);
		void DrawHistoryGraph(uint32_t sampleCount);

		const ResourceSample& SampleAtAge(uint32_t age) const;	// 0 is the newest sample; age must be below the sample count

		static const char* MetricName(Metric metric);
		static std::string FormatMetric(Metric metric, float value);

	private:
		PoolActivity m_MeshActivity;
		PoolActivity m_TextureActivity;

		std::optional<GPUResourceStats> m_SampleStart;	// Empty until the first frame; the capture the running totals are diffed against
		float m_SampleSeconds{ 0.0f };
		double m_LastUploadWaitMs{ 0.0 };
		float m_SampleWorstStallMs{ 0.0f };	// The longest one frame blocked on uploads this sample, which an average would hide

		float m_UploadsPerSecond{ 0.0f };
		float m_UploadBytesPerSecond{ 0.0f };
		float m_WorstStallMs{ 0.0f };

		std::array<ResourceSample, kHistorySize> m_History{};
		uint64_t m_SampleCount{ 0 };	// Samples ever pushed; the newest is at m_SampleCount - 1, wrapped into m_History
		Metric m_SelectedMetric{ Metric::VramUsage };

		std::optional<GPUResourceStats> m_Baseline;	// Marked by the user, to show growth since then
		uint64_t m_BaselineSample{ 0 };				// m_SampleCount when it was marked
	};

}
