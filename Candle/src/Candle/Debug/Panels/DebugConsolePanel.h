#pragma once

#include "Candle/Debug/DebugPanel.h"
#include "Candle/Core/MainThreadLogSink.h"

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
	private:
		void DrawFilterRegion();
		void DrawConsoleRegion();
		void DrawCommandInputRegion();

		void ExecuteCommand(const std::string& commandLine);

	private:
		ConsoleManager& m_ConsoleManager;
		MainThreadLogSink& m_MainThreadLogSink;

		LogListenerHandle m_LogListenerHandle{ 0 };
	};
}