#pragma once

#include "Candle/Debug/DebugPanel.h"
#include "Candle/Core/MainThreadLogSink.h"

#include <glm/glm.hpp>

#include <array>
#include <deque>
#include <span>
#include <string>
#include <vector>

namespace Candle {

	class ConsoleManager;

	// A debug panel that shows a console for logging and executing commands
	class DebugConsolePanel : public DebugPanel
	{
	public:
		DebugConsolePanel(ConsoleManager& consoleManager, MainThreadLogSink& mainThreadLogSink);
		~DebugConsolePanel() override;

		virtual void OnUpdate(const DebugContext& context) override;
		virtual void OnRender(const DebugContext& context) override;

		inline virtual const char* GetName() const override { return "Debug Console"; }
		inline virtual DebugPanelCategory GetCategory() const override { return DebugPanelCategory::General; }
		inline virtual DebugPanelCommand GetCommand() const override
		{
			return DebugPanelCommand{
				.Name{"console"},
				.Description{"Debug console for viewing logs and executing commands"},
				.Usage{"console"}
			};
		}
	private:
		void DrawFilterRegion();
		void DrawConsoleRegion();
		void DrawCommandInputRegion();

		void ExecuteCommand(const std::string& commandLine);

		void AppendRecords(std::span<const LogRecord> records);

	private:
		// One line of a message. Messages with newlines are split so every entry is one line high for the clipper.
		struct LogEntry
		{
			Tick Time{ 0 };
			uint32_t ThreadId{ 0 };
			LogLevel Level{ LogLevel::Info };
			LogChannel Channel{ LogChannel::None };
			std::string Message;
			bool IsContinuationLine{ false };	// A later line of a multi-line message; drawn without the header
		};

		static constexpr size_t kMaxEntries = 4096;

	private:
		ConsoleManager& m_ConsoleManager;
		MainThreadLogSink& m_MainThreadLogSink;

		LogListenerHandle m_LogListenerHandle{ 0 };

		std::deque<LogEntry> m_Entries;
		LogEntry m_Assembling;					// A long message's parts can straddle two dispatches, so this persists between them
		std::vector<uint32_t> m_VisibleEntries;	// Indices into m_Entries that pass the filters, rebuilt each frame

		std::array<bool, kLogLevelCount> m_ShowLevel{ true, true, true, true, true };
		uint16_t m_ChannelMask{ 0xFFFF };		// Display only; separate from Logger's mask so hiding a channel here still logs it elsewhere
		std::array<glm::vec3, kLogChannelCount> m_ChannelColors;

		std::array<char, 256> m_InputBuffer{};
		bool m_ScrollToBottom{ false };
	};
}