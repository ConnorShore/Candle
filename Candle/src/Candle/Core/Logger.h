#pragma once

#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <chrono>
#include <vector>
#include <cstdint>

namespace Candle {

	// TODO later:
	// 1. Need to come up with structure
	//	- Log level
	//  - Channel/Tag)
	//  - LogRecord payload (
	//		- do we store formatted string? 
	//		- What to use as timestamp (std::chrono::system_clock::time_point or Platform Tick?)
	//		- How do we handle console colors?
	// 2. Pushing records to MPSCRingBuffer
	//	- Are records being pushed from multiple threads? If not do we need a MPSCRingBuffer or just SPSCRingBuffer?
	// 3. Draining records from MPSCRingBuffer to console/file on a separate thread
	//	- Need to merge-sort records based on timestamp
	// 4. Setup file logging support (PlatformFile needed?)

	enum class LogLevel : uint8_t
	{
		Trace = 0,
		Info,
		Warn,
		Error,
		Fatal
	};

	// TODO: Maybe just make a string so users can define their own channels?
	enum class Channel : uint16_t
	{
		None			= 0,
		Application		= 1 << 0,
		AI				= 1 << 1,
		Animation		= 1 << 2,
		Render			= 1 << 3,
		Input			= 1 << 4,
		Audio			= 1 << 5,
		Physics			= 1 << 6,
		Memory			= 1 << 7,
		Thread			= 1 << 8,
		Math			= 1 << 9
	};

	static constexpr size_t LogLevelCount = 5;

	struct LogRecord
	{
		LogLevel Level = LogLevel::Info;
		Channel chanel = Channel::None;
		std::string FormattedMessage;
		std::string MessageColor;
		std::chrono::system_clock::time_point Time;
	};

	class Logger
	{
	public:
		Logger(const std::string& name) : m_Name(name) {}
		~Logger() = default;

		//static void InitFileLogging(const std::filesystem::path& filepath);

		// Appends every record newer than cursor and then advances it. Non-destructive, so multiple
		// consumers can each hold their own cursor and a late consumer still sees startup lines.
		static void DrainRecords(uint64_t& cursor, std::vector<LogRecord>& out);

		template <typename... Args>
		inline void PushLog(LogLevel logLevel, Channel channel, std::format_string<Args...> fmt, Args&&... args)
		{
			std::string userMessage = std::format(fmt, std::forward<Args>(args)...);

			Logger::PushRecord(logLevel, channel, m_Name, userMessage);
			
			//// Clean output for the file (No color codes!)
			//std::string cleanOutput = std::format("{}: [{}] {}\n",
			//	m_Name,
			//	GetLogLevelString(logLevel),
			//	userMessage);

			//// Colored output for the console
			//std::string consoleOutput = std::format("{}{}{}",
			//	GetLogLevelColor(logLevel),
			//	cleanOutput,
			//	GetLogLevelResetColor());

			//std::cout << consoleOutput;

			//if (s_LogFile.is_open())
			//{
			//	s_LogFile << cleanOutput;
			//	s_LogFile.flush();
			//}
		}

		inline static bool Enabled(LogLevel logLevel, Channel channel)
		{
			return (logLevel >= s_LogLevel) && (s_ChannelMask & static_cast<uint16_t>(channel)) != 0;
		}

		inline static void SetChannelMask(uint16_t mask) { s_ChannelMask = mask; }

		inline static Logger* CoreLogger()
		{
			static Logger coreLogger("ENGINE");
			return &coreLogger;
		}

		inline static Logger* ClientLogger()
		{
			static Logger clientLogger("APP");
			return &clientLogger;
		}

	private:
		static void PushRecord(LogLevel level, Channel channel, const std::string& loggerName, const std::string& message);

		//const char* GetLogLevelString(LogLevel logLevel);
		//const char* GetLogLevelColor(LogLevel logLevel);
		//const char* GetLogLevelResetColor();

	private:
		std::string m_Name;
		//inline static std::ofstream s_LogFile;
		inline static uint16_t s_ChannelMask = 0xFFFF;		 // All channels enabled by default
		inline static LogLevel s_LogLevel = LogLevel::Info;  // Default log level
	};

}

#define CDL_CORE_TRACE(...) Candle::Logger::CoreLogger()->Log(Ember::LogLevel::Trace, __VA_ARGS__)
#define CDL_CORE_INFO(...)  Candle::Logger::CoreLogger()->Log(Ember::LogLevel::Info, __VA_ARGS__)
#define CDL_CORE_WARN(...)  Candle::Logger::CoreLogger()->Log(Ember::LogLevel::Warn, __VA_ARGS__)
#define CDL_CORE_ERROR(...) Candle::Logger::CoreLogger()->Log(Ember::LogLevel::Error, __VA_ARGS__)
#define CDL_CORE_FATAL(...) Candle::Logger::CoreLogger()->Log(Ember::LogLevel::Fatal, __VA_ARGS__)

#define CDL_TRACE(...) Candle::Logger::ClientLogger()->Log(Ember::LogLevel::Trace, __VA_ARGS__)
#define CDL_INFO(...)  Candle::Logger::ClientLogger()->Log(Ember::LogLevel::Info, __VA_ARGS__)
#define CDL_WARN(...)  Candle::Logger::ClientLogger()->Log(Ember::LogLevel::Warn, __VA_ARGS__)
#define CDL_ERROR(...) Candle::Logger::ClientLogger()->Log(Ember::LogLevel::Error, __VA_ARGS__)
#define CDL_FATAL(...) Candle::Logger::ClientLogger()->Log(Ember::LogLevel::Fatal, __VA_ARGS__)