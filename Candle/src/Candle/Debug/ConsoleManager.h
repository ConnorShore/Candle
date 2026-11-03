#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

namespace Candle {

	using ConsoleCommandCallback = std::function<void(const std::vector<std::string>& args)>;

	// TODO:
	// * Add ability to register commands (i.e. to show debug panels)
	// * Add CVar system to allow changing engine settings at runtime

	// A console command manager that allows registering commands and executing them by name
	// Threading: main thread only; all commands are called and executed on the main thread
	//	 TODO: May see about async calls for the commands but it may not make sense, need to look further into it
	class ConsoleManager
	{
	public:
		ConsoleManager() = default;
		~ConsoleManager() = default;

		void RegisterCommand(const std::string& command, ConsoleCommandCallback callback, const std::string& description = "No Description", const std::string& usage = "Unknown Usage");
		void UnregisterCommand(const std::string& commandName);

		void ExecuteCommand(const std::string& commandLine);

	private:
		void Log(const std::string_view& message);

		struct ConsoleCommand
		{
			ConsoleCommandCallback Callback{};
			std::string Description{ "No Description" };
			std::string Usage{ "Unknown Usage" };
		};

	private:
		std::unordered_map<std::string, ConsoleCommand> m_Commands;
	};

}