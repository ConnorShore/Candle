#pragma once

#include <filesystem>
#include <fstream>
#include <string>

#include "Candle/Core/Logger.h"

namespace Candle {

	class LogSink
	{
	public:
		virtual ~LogSink() = default;
		virtual void Write(const LogRecord* records, size_t count) = 0;
		virtual void Flush() = 0;
	};

	class ConsoleSink : public LogSink
	{
	public:
		ConsoleSink();

		void Write(const LogRecord* records, size_t count) override;
		void Flush() override;

	private:
		std::string m_Batch;	// The batch is built in a single string to avoid multiple writes to stdout
		bool m_Color = false;
	};

	class FileSink : public LogSink
	{
	public:
		explicit FileSink(const std::filesystem::path& path);
		~FileSink() override;

		void Write(const LogRecord* records, size_t count) override;
		void Flush() override;

		bool IsOpen() const { return m_File.is_open(); }

	private:
		std::string m_Batch;
		std::ofstream m_File;
	};

}
