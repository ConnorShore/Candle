#include "cdlpch.h"
#include "ConsoleManager.h"


namespace Candle {

	void ConsoleManager::RegisterCommand(const std::string& command, ConsoleCommandCallback callback, 
										 const std::string& description /*= "No Description"*/, const std::string& usage /*= "Unknown Usage"*/)
	{
		if (m_Commands.find(command) != m_Commands.end())
		{
			CDL_CORE_WARN(LogChannel::Debug, "Console command '{}' is already registered. Overwriting.", command);
		}

		CDL_CORE_INFO(LogChannel::Debug, "Registering console command '{}'", command);
		m_Commands[command] = ConsoleCommand{ 
			.Callback = callback,
			.Description = description,
			.Usage = usage };
	}

	void ConsoleManager::UnregisterCommand(const std::string& command)
	{
		if (m_Commands.find(command) == m_Commands.end())
		{
			CDL_CORE_WARN(LogChannel::Debug, "Console command '{}' is not registered. Cannot unregister.", command);
			return;
		}

		CDL_CORE_INFO(LogChannel::Debug, "Unregistering console command '{}'.", command);
		m_Commands.erase(command);
	}

	void ConsoleManager::ExecuteCommand(const std::string& commandLine)
	{
		if (commandLine.empty())
		{
			CDL_CORE_WARN(LogChannel::Command, "No command entered.");
			return;
		}

		// Tokenize the command line into command and arguments
		// TODO: See if can optimize this somehow without vectors and pushing without known sizes of args and such
		std::istringstream iss(commandLine);
		std::string command;
		std::vector<std::string> args;
		
		// Extract the command
		if (!(iss >> command))
		{
			CDL_CORE_WARN(LogChannel::Command, "Failed to parse command.");
			return;
		}

		// Check to ensure the command is registered
		if (m_Commands.find(command) == m_Commands.end())
		{
			CDL_CORE_WARN(LogChannel::Command, "Command not found: {}", command);
			return;
		}

		// Extract the arguments
		std::string arg;
		while (iss >> arg)
		{
			// TODO: Handle quoted arguments and escape sequences if needed
			args.push_back(arg);
		}

		// Call the registered callback for the command
		m_Commands[command].Callback(args);
	}

}