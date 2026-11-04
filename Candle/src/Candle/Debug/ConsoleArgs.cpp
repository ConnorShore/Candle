#include "cdlpch.h"
#include "ConsoleArgs.h"

namespace Candle {

	ConsoleArgs::ConsoleArgs(std::string_view args)
		: m_Args(std::move(ParseArgs(args)))
	{
	}

	bool ConsoleArgs::Matches(size_t index, std::string_view expected) const
	{
		if (index >= m_Args.size())
			return false;

		return ToLower(m_Args[index].Value) == ToLower(expected);
	}

	size_t ConsoleArgs::GetCount() const
	{
		return m_Args.size();
	}

	std::span<ConsoleArgs::ArgToken> ConsoleArgs::GetAll()
	{
		return m_Args;
	}

	std::optional<std::string_view> ConsoleArgs::TryGetString(size_t index) const
	{
		for (size_t i = 0; i < m_Args.size(); ++i)
		{
			if (i == index)
				return m_Args[i].Value;
		}
		return std::nullopt;
	}

	std::vector<ConsoleArgs::ArgToken> ConsoleArgs::ParseArgs(std::string_view args)
	{
		std::string_view lineView(args);

		std::vector<ConsoleArgs::ArgToken> parsedArgs;
		parsedArgs.reserve(4); // Reserve space for 4 arguments to avoid multiple allocations

		size_t start = lineView.find_first_not_of(" \t");
		if (start == std::string_view::npos)
			return {}; // Empty command line

		while (start < lineView.size())
		{
			bool isQuoted = lineView[start] == '"';
			if (isQuoted)
			{
				// Find the closing quote
				size_t endQuote = lineView.find('"', start + 1);
				if (endQuote == std::string_view::npos)
				{
					CDL_CORE_WARN(LogChannel::Command, "Unmatched quote in command line.");
					return parsedArgs; // Unmatched quote
				}
				std::string_view token = lineView.substr(start + 1, endQuote - start - 1);
				parsedArgs.push_back({
					.Value = token, 
					.StartIndex = static_cast<uint32_t>(start), 
					.EndIndex = static_cast<uint32_t>(endQuote), 
					.IsQuoted = true});
				start = lineView.find_first_not_of(" \t", endQuote + 1);
			}
			else
			{
				size_t end = lineView.find_first_of(" \t", start);
				std::string_view token = lineView.substr(start, end - start);
				parsedArgs.push_back({
					.Value = token, 
					.StartIndex = static_cast<uint32_t>(start), 
					.EndIndex = static_cast<uint32_t>(end), 
					.IsQuoted = false});
				start = lineView.find_first_not_of(" \t", end);
			}
		}

		return parsedArgs;
	}

}