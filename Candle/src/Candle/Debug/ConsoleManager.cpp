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
		m_Commands.push_back(ConsoleCommandSpec{});	// Reserve index 0 for invalid handle)
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
		CDL_CORE_ASSERT(IsValidCommandName(commandSpec.CommandParts), "Command name parts can't be empty or contain the delimiter or a space.");

		m_Commands.push_back(std::move(commandSpec));
		m_CommandLookup[GetCommandKey(m_Commands.back().CommandParts)] = m_NextCommandHandle;
		return m_NextCommandHandle++;
	}

	void ConsoleManager::UnregisterCommand(ConsoleCommandHandle handle)
	{
		ConsoleCommandSpec& commandSpec = m_Commands[handle];
		m_CommandLookup.erase(GetCommandKey(commandSpec.CommandParts));
		m_Commands[handle] = {};	// Reset the command spec to default
	}

	AutoCompleteResult ConsoleManager::AutoCompleteCommand(std::string_view input) const
	{
		// TODO: Implement this in the future
		CDL_CORE_ERROR(LogChannel::Command, "AutoCompleteCommand is not implemented yet.");
		return {};
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

	bool ConsoleManager::ContainsCommand(const std::string_view command) const
	{
		return m_CommandLookup.contains(ToLower(command));
	}

	bool ConsoleManager::IsValidCommandName(std::span<const std::string> nameParts)
	{
		return !nameParts.empty() && std::ranges::all_of(nameParts, [](std::string_view part) {
			return !part.empty() && !part.contains(kCommandDelimiter) && !part.contains(' ');
			});
	}

	std::string ConsoleManager::GetCommandKey(std::span<const std::string> nameParts)
	{
		return ToLower(nameParts | std::views::join_with(kCommandDelimiter) | std::ranges::to<std::string>());
	}
}