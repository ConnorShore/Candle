#include "cdlpch.h"
#include "LogSink.h"

#include <cstdio>
#include <iterator>

#ifdef CDL_PLATFORM_WINDOWS
	#include <Windows.h>
#endif

namespace Candle {

	namespace {

		constexpr const char* s_LevelColors[LogLevelCount] = {
			"\x1b[90m",		// Trace - grey
			"\x1b[37m",		// Info  - white
			"\x1b[33m",		// Warn  - yellow
			"\x1b[31m",		// Error - red
			"\x1b[1;41m"	// Fatal - bold red
		};

		constexpr const char* s_ColorReset = "\x1b[0m";

		void AppendRecord(std::string& out, const LogRecord& record, bool color)
		{
			const uint64_t milliseconds = Platform::ToUnixMicroseconds(record.Time) / 1000;
			const uint64_t seconds = milliseconds / 1000;

			if (color)
				out.append(s_LevelColors[static_cast<size_t>(record.Level)]);

			std::format_to(std::back_inserter(out), "[{:02}:{:02}:{:02}.{:03}] [{}] [{}] [{:<5}] (T{}) {}",
				(seconds / 3600) % 24,
				(seconds / 60) % 60,
				seconds % 60,
				milliseconds % 1000,
				LoggerTypeName(record.Type),
				LogChannelName(record.Channel),
				LogLevelName(record.Level),
				record.ThreadId,
				record.Message);

			if (color)
				out.append(s_ColorReset);

			out.push_back('\n');
		}

	}

	ConsoleSink::ConsoleSink()
	{
		m_Batch.reserve(64 * 1024);
		m_Color = Platform::EnableConsoleAnsiColors();
	}

	void ConsoleSink::Write(const LogRecord* records, size_t count)
	{
		m_Batch.clear();

		for (size_t i = 0; i < count; ++i)
			AppendRecord(m_Batch, records[i], m_Color);

		// One write per batch rather than per record
		std::fwrite(m_Batch.data(), 1, m_Batch.size(), stdout);
	}

	void ConsoleSink::Flush()
	{
		std::fflush(stdout);
	}

	FileSink::FileSink(const std::filesystem::path& path)
	{
		m_Batch.reserve(64 * 1024);

		std::error_code error;
		if (const std::filesystem::path parent = path.parent_path(); !parent.empty())
			std::filesystem::create_directories(parent, error);

		// Binary mode avoids CRLF translation on Windows, which would corrupt the log if it contained a lone LF
		m_File.open(path, std::ios::out | std::ios::trunc | std::ios::binary);

		if (!m_File.is_open())
			std::fprintf(stderr, "FileSink: failed to open '%s'\n", path.string().c_str());
	}

	FileSink::~FileSink()
	{
		Flush();
	}

	void FileSink::Write(const LogRecord* records, size_t count)
	{
		if (!m_File.is_open())
			return;

		m_Batch.clear();

		for (size_t i = 0; i < count; ++i)
			AppendRecord(m_Batch, records[i], false);

		m_File.write(m_Batch.data(), static_cast<std::streamsize>(m_Batch.size()));
	}

	void FileSink::Flush()
	{
		if (m_File.is_open())
			m_File.flush();
	}

}
