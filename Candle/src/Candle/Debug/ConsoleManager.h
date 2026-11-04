#pragma once

#include "ConsoleTypes.h"

#include <vector>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <span>
#include <cstdint>

namespace Candle {

	using ConsoleCommandHandle = uint64_t;
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

	// A console command manager that allows registering commands and executing them by name
	// Threading: main thread only; all commands are called and executed on the main thread
	class ConsoleManager
	{
	public:
		inline static constexpr std::string_view kCommandDelimiter = ".";

	public:
		ConsoleManager();
		~ConsoleManager();

		ConsoleCommandHandle RegisterCommand(ConsoleCommandSpec commandSpec);
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
		std::unordered_map<std::string, ConsoleCommandHandle> m_CommandLookup;	// Maps lowercased command names to their handles

		ConsoleCommandHandle m_NextCommandHandle{ 1 };	// Start at 1, 0 is reserved for invalid handle
	};

}