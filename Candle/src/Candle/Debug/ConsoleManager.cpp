#include "cdlpch.h"
#include "ConsoleManager.h"

#include <sstream>
#include <vector>
#include <algorithm>
#include <ranges>
#include <string_view>
#include <tuple>

namespace Candle {

	ConsoleManager::ConsoleManager()
	{
		m_Commands.push_back({});	// Reserve index 0 for invalid handle
	}

	ConsoleManager::~ConsoleManager()
	{
		for (size_t i = 1; i < m_Commands.size(); ++i)
		{
			if (m_Commands[i].Callback)
				UnregisterCommand(i);
		}
	}

	ConsoleCommandHandle ConsoleManager::RegisterCommand(ConsoleCommandSpec commandSpec)
	{
		m_Commands.push_back(std::move(commandSpec));
		m_CommandLookup[ToLower(m_Commands.back().Name)] = m_NextCommandHandle;
		return m_NextCommandHandle++;
	}

	AutoCompleteResult ConsoleManager::AutoCompleteCommand(std::string_view input) const
	{
		// TODO: Implement this in the future
		CDL_CORE_ERROR(LogChannel::Command, "AutoCompleteCommand is not implemented yet.");
		return {};
	}

	bool ConsoleManager::ContainsCommand(const std::string_view command) const
	{
		return m_CommandLookup.contains(ToLower(command));
	}

	void ConsoleManager::UnregisterCommand(ConsoleCommandHandle handle)
	{
		const std::string commandName = ToLower(m_Commands[handle].Name);
		m_CommandLookup.erase(commandName);
		m_Commands[handle] = {};	// Reset the command spec to default
	}

	void ConsoleManager::ExecuteCommand(const std::string_view commandLine)
	{
		if (commandLine.empty())
		{
			CDL_CORE_WARN(LogChannel::Command, "No command entered.");
			return;
		}

		size_t spaceIndex = commandLine.find(' ');
		bool hasArgs = spaceIndex != std::string_view::npos;
		std::string_view command = commandLine.substr(0, spaceIndex);
		std::string_view argsStr = hasArgs ? commandLine.substr(spaceIndex + 1) : std::string_view{};

		if (!ContainsCommand(command))
		{
			CDL_CORE_WARN(LogChannel::Command, "Unknown command: {}", command);
			return;
		}

		// Call the registered callback for the command
		ConsoleCommandHandle handle = m_CommandLookup.at(ToLower(command));
		ConsoleArgs consoleArgs{ argsStr };

		m_Commands[handle].Callback(std::move(consoleArgs));
	}

}