#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>

#include "Candle/Core/Asserts.h"
#include "Candle/Core/Memory/ScopedPtr.h"
#include "Candle/Platform/Platform.h"

namespace Candle {

	class LogSink;

	enum class LogLevel : uint8_t
	{
		Trace = 0,
		Info,
		Warn,
		Error,
		Fatal
	};

	static constexpr size_t LogLevelCount = 5;

	enum class LogChannel : uint16_t
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
		Math			= 1 << 9,
	};

	// Bitwise OR/AND operators for LogChannel to allow combining channels
inline LogChannel operator|(LogChannel lhs, LogChannel rhs) { return static_cast<LogChannel>(static_cast<uint16_t>(lhs) | static_cast<uint16_t>(rhs)); }
	inline uint16_t operator&(LogChannel lhs, LogChannel rhs) { return static_cast<uint16_t>(lhs) & static_cast<uint16_t>(rhs); }

	struct LogRecord
	{
		Tick Time = { 0 };
		uint32_t ThreadId = 0;
		uint8_t Type = 0; // 0 = Core, 1 = Application
		LogLevel Level = LogLevel::Info;
		LogChannel Channel = LogChannel::None;
		char Message[240] = { 0 };
	};
	CDL_STATIC_ASSERT(sizeof(LogRecord) == 256, "LogRecord size is not 256 bytes. Check for padding or alignment issues.");	// 256 is 4 cache lines

	// Simple display helpers (safe to call from any thread)
	const char* LogLevelName(LogLevel level);
	const char* LogChannelName(LogChannel channel);
	const char* LoggerTypeName(uint8_t type);

	struct LoggerSpecification
	{
		LogLevel Level = LogLevel::Info;
		uint16_t ChannelMask = 0xFFFF;
		bool LogToConsole = true;
		bool LogToFile = false;
		std::filesystem::path LogFilePath = "logs/Candle.log";
	};

	class Logger
	{
	public:
		explicit Logger(uint8_t type) : m_Type(type) {}
		~Logger() = default;

		static void Init(const LoggerSpecification& spec = {});
		static void Shutdown();

		static void AddSink(ScopedPtr<LogSink> sink);
		static bool Flush();

		inline static bool Enabled(LogLevel level, LogChannel channel)
		{
			return level >= s_Level.load(std::memory_order_relaxed)
				&& (s_ChannelMask.load(std::memory_order_relaxed) & static_cast<uint16_t>(channel)) != 0;
		}

		// Use relaxed ordering because the drain thread is the only one that writes to these, and it does so before it starts draining.
		inline static void SetLevel(LogLevel level) { s_Level.store(level, std::memory_order_relaxed); }
		inline static void SetChannelMask(uint16_t mask) { s_ChannelMask.store(mask, std::memory_order_relaxed); }

		// Any thread. Formats into the record in place, hands it to the ring, and returns.
		// The only blocking path is Fatal, which waits for the sinks because a crash usually follows it.
		template <typename... Args>
		void PushLog(LogLevel level, LogChannel channel, std::format_string<Args...> fmt, Args&&... args)
		{
			LogRecord record;
			record.Time = Platform::GetTick();
			record.ThreadId = Platform::GetCurrentThreadId();
			record.Type = m_Type;
			record.Level = level;
			record.Channel = channel;

			// Set capacity to size-1 to leave room for the null terminator.
			constexpr size_t capacity = sizeof(LogRecord::Message) - 1;

			auto result = std::format_to_n(record.Message, capacity, fmt, std::forward<Args>(args)...);
			*result.out = '\0';

			if (static_cast<size_t>(result.size) > capacity)
				std::memcpy(record.Message + capacity - 3, "...", 3);

			PushRecord(record);

			if (level == LogLevel::Fatal && !Flush())
				EmergencyWrite(record);
		}

		inline static Logger* CoreLogger()
		{
			static Logger s_CoreLogger(0);
			return &s_CoreLogger;
		}

		inline static Logger* ClientLogger()
		{
			static Logger s_ClientLogger(1);
			return &s_ClientLogger;
		}

	private:
		static void PushRecord(const LogRecord& record);

		// Last resort when the drain thread cannot be reached: straight to stderr, unsynchronised,
		// on the assumption the process is about to die anyway.
		static void EmergencyWrite(const LogRecord& record);

	private:
		uint8_t m_Type = 0;

		inline static std::atomic<LogLevel> s_Level{ LogLevel::Info };
		inline static std::atomic<uint16_t> s_ChannelMask{ 0xFFFF };
	};

}

// Only push a log if the level and channel are enabled, to avoid formatting costs
#define CDL_LOG_IMPL(loggerFn, level, channel, ...)                                    \
	do {                                                                               \
		if (Candle::Logger::Enabled(level, channel))                                 \
			Candle::Logger::loggerFn()->PushLog(level, channel, __VA_ARGS__);        \
	} while (false)

#if defined(CDL_DIST)
	#define CDL_CORE_TRACE(channel, ...) ((void)0)
	#define CDL_TRACE(channel, ...)      ((void)0)
#else
	#define CDL_CORE_TRACE(channel, ...) CDL_LOG_IMPL(CoreLogger,   Candle::LogLevel::Trace, channel, __VA_ARGS__)
	#define CDL_TRACE(channel, ...)      CDL_LOG_IMPL(ClientLogger, Candle::LogLevel::Trace, channel, __VA_ARGS__)
#endif

#define CDL_CORE_INFO(channel, ...)  CDL_LOG_IMPL(CoreLogger,   Candle::LogLevel::Info,  channel, __VA_ARGS__)
#define CDL_CORE_WARN(channel, ...)  CDL_LOG_IMPL(CoreLogger,   Candle::LogLevel::Warn,  channel, __VA_ARGS__)
#define CDL_CORE_ERROR(channel, ...) CDL_LOG_IMPL(CoreLogger,   Candle::LogLevel::Error, channel, __VA_ARGS__)
#define CDL_CORE_FATAL(channel, ...) CDL_LOG_IMPL(CoreLogger,   Candle::LogLevel::Fatal, channel, __VA_ARGS__)

#define CDL_INFO(channel, ...)       CDL_LOG_IMPL(ClientLogger, Candle::LogLevel::Info,  channel, __VA_ARGS__)
#define CDL_WARN(channel, ...)       CDL_LOG_IMPL(ClientLogger, Candle::LogLevel::Warn,  channel, __VA_ARGS__)
#define CDL_ERROR(channel, ...)      CDL_LOG_IMPL(ClientLogger, Candle::LogLevel::Error, channel, __VA_ARGS__)
#define CDL_FATAL(channel, ...)      CDL_LOG_IMPL(ClientLogger, Candle::LogLevel::Fatal, channel, __VA_ARGS__)
