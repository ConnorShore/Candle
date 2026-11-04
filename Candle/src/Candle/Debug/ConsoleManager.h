#pragma once

#include "ConsoleTypes.h"
#include "ScopedCommandHandle.h"

#include <vector>
#include <string_view>
#include <map>
#include <functional>
#include <span>
#include <cstdint>

namespace Candle {

	struct ConsoleCommandSpec
	{
		std::vector<std::string> CommandParts;	// Either a single name or multiple names for hierarchical commands
		std::string Description{ "No Description" };
		std::string Usage{ "Unknown Usage" };
		ConsoleCommandCallback Callback{};
	};

	struct AutoCompleteResult
	{
		uint32_t ReplaceBegin{ 0 };					// Index of the first character to replace in the input string
		uint32_t ReplaceEnd{ 0 };					// Index of the last character to replace in the input string
		std::span<std::string_view> Suggestions;	// List of suggestions to replace the input string with
	};

	class ConsoleManager;

	// A console command manager that allows registering commands and executing them by name
	// Threading: main thread only; all commands are called and executed on the main thread
	class ConsoleManager
	{
	public:
		inline static constexpr std::string_view kCommandDelimiter = ".";

	public:
		ConsoleManager();
		~ConsoleManager();

		[[nodiscard]] ScopedCommandHandle RegisterCommand(ConsoleCommandSpec commandSpec);
		void UnregisterCommand(ConsoleCommandHandle handle);
		[[nodiscard]] AutoCompleteResult AutoCompleteCommand(std::string_view input) const;	// TODO: Implement this in the future
		
		void ExecuteCommand(const std::string_view commandLine);

	private:
		bool ContainsCommand(const std::string_view command) const;

		[[maybe_unused]] bool IsValidCommandName(std::span<const std::string> nameParts);
		std::string GetCommandKey(std::span<const std::string> nameParts);

	private:
		// Keeping here for now, may add a CommandRegistry in the future (maybe when cvar system is added)
		std::vector<ConsoleCommandSpec> m_Commands;	// Index is the command handle, starting at 1; 0 is reserved for invalid handle
		std::map<std::string, ConsoleCommandHandle> m_CommandLookup;	// Lowercase names. Sorted map so commands with same prefix can sit together

		ConsoleCommandHandle m_NextCommandHandle{ kInvalidCommandHandle + 1 };	// Start at 1, 0 is reserved for invalid handle
	};

}