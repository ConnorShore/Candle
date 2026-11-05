#pragma once

#include <string_view>
#include <span>
#include <concepts>
#include <optional>
#include <vector>
#include <type_traits>
#include <string>

namespace Candle {

	template<typename T>
	concept IsConsoleArgType = std::is_arithmetic_v<T> || std::is_same_v<T, std::string>;

	class ConsoleArgs
	{
	public:
		struct ArgToken
		{
			std::string_view Value;
			uint32_t StartIndex;
			uint32_t EndIndex;
			bool IsQuoted;
		};

	public:
		ConsoleArgs(std::string_view args);
		~ConsoleArgs() = default;

		template<IsConsoleArgType T>
		[[nodiscard]] inline std::optional<T> TryGet(size_t index) const
		{
			if (index >= m_Args.size())
				return std::nullopt;

			const std::string_view arg = m_Args[index].Value;
			if constexpr (std::is_same_v<T, std::string>)
			{
				return std::string(arg);
			}
			else if constexpr (std::is_arithmetic_v<T>)
			{
				try
				{
					if constexpr (std::is_integral_v<T>)
					{
						return static_cast<T>(std::stoll(std::string(arg)));
					}
					else if constexpr (std::is_floating_point_v<T>)
					{
						return static_cast<T>(std::stod(std::string(arg)));
					}
				}
				catch (const std::exception&)
				{
					CDL_CORE_WARN(LogChannel::Command, "Failed to convert argument {} to type {}", index, typeid(T).name());
					return std::nullopt;
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] bool Matches(size_t index, std::string_view expected) const;

		[[nodiscard]] size_t GetCount() const;
		[[nodiscard]] std::span<ArgToken> GetAll();
		[[nodiscard]] std::optional<std::string_view> TryGetString(size_t index) const;

	private:
		std::vector<ArgToken> ParseArgs(std::string_view args);

	private:
		std::vector<ArgToken> m_Args;
	};


}