#pragma once

#include "Candle/Debug/DebugPanel.h"

#include <vector>
#include <array>
#include <cstdint>
#include <optional>

struct ImDrawList;
struct ImVec2;

namespace Candle {

	class FrameGraphPanel : public DebugPanel
	{
	public:
		FrameGraphPanel() = default;
		~FrameGraphPanel() override = default;

		virtual void OnUpdate(const DebugContext& context) override;
		virtual void OnRender(const DebugContext& context) override;

		inline virtual const char* GetName() const override { return "Frame Graph"; }
		inline virtual DebugPanelCategory GetCategory() const override { return DebugPanelCategory::General; }
		inline virtual DebugPanelCommand GetCommand() override
		{
			return DebugPanelCommand{
				.Name{ "FrameGraph" },
				.Description{ "Frame timing statistics and graphs" },
				.Usage{ "FrameGraph" },
				.Callback{ nullptr }	// Opt for default callback to open/close for now
			};
		}

	private:
		constexpr static size_t kMaxHistorySize = 360;	// Few seconds at common frame times
		struct StatHistory
		{
			std::array<float, kMaxHistorySize> Values{ 0.0f };
			uint32_t CurrentIndex{ 0 };
			uint32_t Count{ 0 };

			void Push(float value);
			float Max();
			float Min();
			float Average();
			float AtAge(uint32_t age) const;	// 0 is the newest sample; age must be below Count
		};

		// One frame's samples, copied out when clicked so the details outlive the frame scrolling off the graph.
		// Main and Render come from the same push, so Render is a few frames older than Main (see DebugContext).
		struct FrameSample
		{
			uint64_t SampleNumber{ 0 };		// Which push it came from, counting from the panel's first frame
			ThreadFrameTimings Main;
			ThreadFrameTimings Render;
		};

		void RenderWindow();
		void DrawUnitGraph(ImDrawList* drawList, const ImVec2& min, const ImVec2& size, std::optional<uint32_t> hoveredAge);	// Both threads' frame times on one scale, with min/avg/max lines
		void DrawFrameDetails();

		std::optional<uint32_t> SampleAgeAt(float x, float graphMinX, float graphWidth) const;	// The sample under x, if one has been written there
		FrameSample CaptureFrame(uint32_t age) const;

	private:
		StatHistory m_RenderFrameTimeHistory, m_RenderWaitTimeHistory;
		StatHistory m_MainFrameTimeHistory, m_MainWaitTimeHistory;

		float m_GraphScaleMax{ 16.7f };		// Initially 60 FPS, but will auto-scale up if needed
		uint64_t m_SampleCount{ 0 };				// Pushes so far, to follow the selected frame as it scrolls
		std::optional<FrameSample> m_SelectedFrame;	// Clicked in the graph; shown in the details pane

		bool m_Paused{ false };		// Stops OnUpdate pushing samples, so the graph holds still for picking frames
		bool m_ShowMain{ true }, m_ShowRender{ true };				// Each thread's frame time line and its min/avg/max
		bool m_ShowMainWait{ false }, m_ShowRenderWait{ false };	// Each thread's FramePump wait, as bars under its line
	};

}