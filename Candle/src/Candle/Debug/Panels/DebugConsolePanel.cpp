#include "cdlpch.h"
#include "DebugConsolePanel.h"

#include "Candle/Debug/ConsoleManager.h"

#include <imgui.h>

#include <bit>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace Candle {

	namespace {

		constexpr float kGoldenRatioConjugate = 0.618034f;	// Hue step that keeps neighboring channels far apart

		// Matches ConsoleSink's ANSI colors. Info has none there, so 0 here means the style's text color.
		constexpr ImU32 kLevelColors[kLogLevelCount] = {
			IM_COL32(140, 140, 140, 255),	// Trace
			0,								// Info
			IM_COL32(230, 200, 80, 255),	// Warn
			IM_COL32(240, 90, 90, 255),		// Error
			IM_COL32(255, 40, 40, 255)		// Fatal
		};

		ImU32 LevelColor(LogLevel level)
		{
			const ImU32 color = kLevelColors[static_cast<size_t>(level)];
			return color != 0 ? color : ImGui::GetColorU32(ImGuiCol_Text);
		}

		glm::vec3 DefaultChannelColor(size_t index)
		{
			const float hue = std::fmod(static_cast<float>(index) * kGoldenRatioConjugate, 1.0f);
			glm::vec3 color;
			ImGui::ColorConvertHSVtoRGB(hue, 0.55f, 0.95f, color.r, color.g, color.b);
			return color;
		}

	}

	DebugConsolePanel::DebugConsolePanel(ConsoleManager& consoleManager, MainThreadLogSink& mainThreadLogSink)
		: m_ConsoleManager(consoleManager)
		, m_MainThreadLogSink(mainThreadLogSink)
	{
		for (size_t i = 0; i < kLogChannelCount; ++i)
			m_ChannelColors[i] = DefaultChannelColor(i);

		m_LogListenerHandle = m_MainThreadLogSink.RegisterListener(
			[this](std::span<const LogRecord> records) { AppendRecords(records); });
	}

	DebugConsolePanel::~DebugConsolePanel()
	{
		m_MainThreadLogSink.UnregisterListener(m_LogListenerHandle);
	}

	void DebugConsolePanel::OnUpdate(const DebugContext& context)
	{
		// Show with "~" key
		if (context.Input.IsKeyPressed(KeyCode::GraveAccent)) 
		{
			IsOpen = !IsOpen;

			if (IsOpen)
				m_FocusInput = true;
		}
	}

	void DebugConsolePanel::OnRender(const DebugContext& context)
	{
		ImGui::SetNextWindowSize({ 800.0f, 400.0f }, ImGuiCond_FirstUseEver);
		if (m_FocusInput)
			ImGui::SetNextWindowFocus();

		if (ImGui::Begin(GetName(), &IsOpen))
		{
			DrawFilterRegion();
			ImGui::Separator();
			DrawConsoleRegion();
			ImGui::Separator();
			DrawCommandInputRegion();
		}
		ImGui::End();
	}

	void DebugConsolePanel::DrawFilterRegion()
	{
		for (size_t i = 0; i < kLogLevelCount; ++i)
		{
			const LogLevel level = static_cast<LogLevel>(i);
			ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(level));
			ImGui::Checkbox(LogLevelName(level), &m_ShowLevel[i]);
			ImGui::PopStyleColor();
			ImGui::SameLine();
		}

		// ### keeps the ID stable while the count in the label changes
		char channelsLabel[32];
		std::snprintf(channelsLabel, sizeof(channelsLabel), "Channels (%d/%zu)###Channels", std::popcount(m_ChannelMask), kLogChannelCount);
		if (ImGui::Button(channelsLabel))
			ImGui::OpenPopup("##ChannelFilter");

		ImGui::SameLine();
		const float clearWidth = ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(ImGui::GetContentRegionAvail().x - clearWidth, 0.0f));
		if (ImGui::Button("Clear"))
			m_Entries.clear();

		if (ImGui::BeginPopup("##ChannelFilter"))
		{
			if (ImGui::Button("All"))
				m_ChannelMask = 0xFFFF;
			ImGui::SameLine();
			if (ImGui::Button("None"))
				m_ChannelMask = 0;
			ImGui::SameLine();
			if (ImGui::Button("Reset Colors"))
			{
				for (size_t i = 0; i < kLogChannelCount; ++i)
					m_ChannelColors[i] = DefaultChannelColor(i);
			}
			ImGui::Separator();

			for (size_t i = 0; i < kLogChannelCount; ++i)
			{
				const uint16_t bit = static_cast<uint16_t>(1u << i);
				bool shown = (m_ChannelMask & bit) != 0;

				ImGui::PushID(static_cast<int>(i));
				if (ImGui::Checkbox("##Show", &shown))
					m_ChannelMask ^= bit;
				ImGui::SameLine();
				ImGui::ColorEdit3("##Color", &m_ChannelColors[i].r, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
				ImGui::SameLine();
				ImGui::TextUnformatted(LogChannelName(static_cast<LogChannel>(bit)));
				ImGui::PopID();
			}
			ImGui::EndPopup();
		}
	}

	void DebugConsolePanel::DrawConsoleRegion()
	{
		// Leaves room below for the separator and the input line
		const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("##ConsoleRegion", { 0.0f, -footerHeight }, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
		{
			m_VisibleEntries.clear();
			for (uint32_t i = 0; i < static_cast<uint32_t>(m_Entries.size()); ++i)
			{
				const LogEntry& entry = m_Entries[i];
				const bool channelShown = entry.Channel == LogChannel::None || (m_ChannelMask & static_cast<uint16_t>(entry.Channel)) != 0;
				if (channelShown && m_ShowLevel[static_cast<size_t>(entry.Level)])
					m_VisibleEntries.push_back(i);
			}

			const float spaceWidth = ImGui::CalcTextSize(" ").x;
			const float timestampWidth = ImGui::CalcTextSize("[00:00:00.000]").x + spaceWidth;

			auto drawEntry = [&](const LogEntry& entry)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(entry.Level));

				if (entry.IsContinuationLine)
				{
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + timestampWidth);
					ImGui::TextUnformatted(entry.Message.data(), entry.Message.data() + entry.Message.size());
					ImGui::PopStyleColor();
					return;
				}

				const uint64_t milliseconds = Platform::ToUnixMicroseconds(entry.Time) / 1000;
				const uint64_t seconds = milliseconds / 1000;
				char timestamp[32];
				auto result = std::format_to_n(timestamp, sizeof(timestamp) - 1, "[{:02}:{:02}:{:02}.{:03}]",
					(seconds / 3600) % 24, (seconds / 60) % 60, seconds % 60, milliseconds % 1000);
				*result.out = '\0';

				ImGui::TextDisabled("%s", timestamp);
				ImGui::SameLine(0.0f, spaceWidth);

				ImVec4 channelColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
				if (entry.Channel != LogChannel::None)
				{
					const glm::vec3& color = m_ChannelColors[std::countr_zero(static_cast<uint16_t>(entry.Channel))];
					channelColor = { color.r, color.g, color.b, 1.0f };
				}
				ImGui::TextColored(channelColor, "[%s]", LogChannelName(entry.Channel));
				ImGui::SameLine(0.0f, spaceWidth);

				ImGui::Text("[%-5s]", LogLevelName(entry.Level));
				ImGui::SameLine(0.0f, spaceWidth);
				ImGui::TextUnformatted(entry.Message.data(), entry.Message.data() + entry.Message.size());
				ImGui::SetItemTooltip("Thread %u", entry.ThreadId);

				ImGui::PopStyleColor();
			};

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { ImGui::GetStyle().ItemSpacing.x, 1.0f });
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(m_VisibleEntries.size()));
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
					drawEntry(m_Entries[m_VisibleEntries[i]]);
			}
			ImGui::PopStyleVar();

			// Follows new lines while the view is at the bottom; scrolling up stops it
			if (m_ScrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				ImGui::SetScrollHereY(1.0f);
			m_ScrollToBottom = false;
		}
		ImGui::EndChild();
	}

	void DebugConsolePanel::DrawCommandInputRegion()
	{
		// TODO: Autocomplete and suggestions from ConsoleManager's commands (ImGuiInputTextFlags_CallbackCompletion), and history (CallbackHistory)
		//  Should be selectable items that will autofill if selected (if hovered it will show a tooltip with the description and usage)
		ImGui::SetNextItemWidth(-FLT_MIN);

		// Cleared here rather than in OnRender, so a collapsed window still focuses the input once it is expanded
		if (m_FocusInput)
		{
			ImGui::SetKeyboardFocusHere();
			m_FocusInput = false;
		}
		const bool submitted = ImGui::InputTextWithHint("##CommandInput", "Enter a command", m_InputBuffer.data(), m_InputBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SetItemDefaultFocus();

		if (!submitted)
			return;

		const std::string_view line = m_InputBuffer.data();
		const size_t first = line.find_first_not_of(" \t");
		if (first != std::string_view::npos)
		{
			const size_t last = line.find_last_not_of(" \t");
			ExecuteCommand(std::string(line.substr(first, last - first + 1)));
			m_ScrollToBottom = true;
		}

		m_InputBuffer[0] = '\0';
		ImGui::SetKeyboardFocusHere(-1);	// Enter drops focus; take it back so the next command can be typed straight away
	}

	void DebugConsolePanel::AppendRecords(std::span<const LogRecord> records)
	{
		for (const LogRecord& record : records)
		{
			if (!record.IsContinuation)
			{
				m_Assembling.Time = record.Time;
				m_Assembling.ThreadId = record.ThreadId;
				m_Assembling.Level = record.Level;
				m_Assembling.Channel = record.Channel;
				m_Assembling.Message.assign(record.Message);
			}
			else
			{
				m_Assembling.Message.append(record.Message);
			}

			if (record.HasNext)
				continue;

			const std::string_view message = m_Assembling.Message;
			size_t lineStart = 0;
			while (true)
			{
				const size_t lineEnd = message.find('\n', lineStart);
				const bool isLastLine = lineEnd == std::string_view::npos;

				// A trailing newline would otherwise leave an empty last line
				if (isLastLine && lineStart != 0 && lineStart == message.size())
					break;

				m_Entries.push_back(LogEntry{
					.Time = m_Assembling.Time,
					.ThreadId = m_Assembling.ThreadId,
					.Level = m_Assembling.Level,
					.Channel = m_Assembling.Channel,
					.Message = std::string(message.substr(lineStart, isLastLine ? std::string_view::npos : lineEnd - lineStart)),
					.IsContinuationLine = lineStart != 0 });

				if (isLastLine)
					break;

				lineStart = lineEnd + 1;
			}
		}

		while (m_Entries.size() > kMaxEntries)
			m_Entries.pop_front();
	}

	void DebugConsolePanel::ExecuteCommand(const std::string& commandLine)
	{
		m_ConsoleManager.ExecuteCommand(commandLine);
	}

}