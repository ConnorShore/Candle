#include "cdlpch.h"
#include "DebugConsolePanel.h"

#include "Candle/Debug/ConsoleManager.h"

#include <imgui.h>
#include <iostream>

namespace Candle {

	DebugConsolePanel::DebugConsolePanel(ConsoleManager& consoleManager, MainThreadLogSink& mainThreadLogSink)
		: m_ConsoleManager(consoleManager)
		, m_MainThreadLogSink(mainThreadLogSink)
	{
		m_LogListenerHandle = m_MainThreadLogSink.RegisterListener(
			[this](std::span<const LogRecord> records) { 
				for (const LogRecord& record : records)
				{
					std::string message;
					message.reserve(1024);
					std::cout << "DEBUG CONSOLE PANEL: " << record.Message << std::endl;
				}
			});
	}

	DebugConsolePanel::~DebugConsolePanel()
	{
		m_MainThreadLogSink.UnregisterListener(m_LogListenerHandle);
	}

	void DebugConsolePanel::OnUpdate(const DebugContext& context)
	{

	}

	void DebugConsolePanel::OnRender(const DebugContext& context)
	{

	}

}