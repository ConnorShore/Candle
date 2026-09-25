#include "cdlpch.h"
#include "Logger.h"

#include "Candle/Core/LogSink.h"
#include "Candle/Core/Threading/MPSCRingBuffer.h"

#include <bit>
#include <cstdio>
#include <thread>
#include <vector>

#ifdef CDL_PLATFORM_WINDOWS
	#include <Windows.h>
#endif

namespace Candle {

	namespace {

		// 8192 * 256 B = 2 MB
		constexpr size_t s_RingCapacity = 8192;

		// 128 * 256 B = 32 KB
		constexpr size_t s_DrainBatch = 128;

		constexpr uint64_t s_FlushIntervalMicroseconds = 100'000;
		constexpr uint64_t s_FlushTimeoutMicroseconds = 2'000'000;

		MPSCRingBuffer<LogRecord>& Ring()
		{
			static MPSCRingBuffer<LogRecord>* s_Ring = new MPSCRingBuffer<LogRecord>(s_RingCapacity);
			return *s_Ring;
		}

		std::vector<Scoped<LogSink>>& Sinks()
		{
			static std::vector<Scoped<LogSink>> s_Sinks;
			return s_Sinks;
		}

		std::thread s_DrainThread;
		std::atomic<bool> s_Running{ false };

		// Any thread. Incremented to request a flush; the drain thread will increment s_FlushComplete when done.
		std::atomic<uint64_t> s_FlushRequest{ 0 };
		std::atomic<uint64_t> s_FlushComplete{ 0 };

		uint64_t s_DroppedReported = 0;

		// Drain thread only. Empties the ring into every sink; returns how many records moved.
		size_t DrainOnce()
		{
			LogRecord batch[s_DrainBatch];
			size_t total = 0;

			while (true)
			{
				const size_t count = Ring().PopBatch(batch, s_DrainBatch);
				if (count == 0)
					break;

				for (const Scoped<LogSink>& sink : Sinks())
					sink->Write(batch, count);

				total += count;
			}

			return total;
		}

		// Drain thread only. The ring counts drops but cannot report them; without this, loss is silent.
		void ReportDrops()
		{
			const uint64_t dropped = Ring().DroppedCount();
			if (dropped == s_DroppedReported)
				return;

			LogRecord record;
			record.Time = Platform::GetTick();
			record.ThreadId = Platform::GetCurrentThreadId();
			record.Level = LogLevel::Warn;
			record.Channel = LogChannel::Application;

			auto result = std::format_to_n(record.Message, sizeof(record.Message) - 1,
				"Logger dropped {} record(s) -- the ring filled faster than it drained.",
				dropped - s_DroppedReported);
			*result.out = '\0';

			for (const Scoped<LogSink>& sink : Sinks())
				sink->Write(&record, 1);

			s_DroppedReported = dropped;
		}

		void DrainThreadMain()
		{
			uint64_t idleSpins = 0;
			Tick lastFlush = Platform::GetTick();

			while (true)
			{
				const uint64_t request = s_FlushRequest.load(std::memory_order_acquire);
				const bool running = s_Running.load(std::memory_order_acquire);

				const size_t drained = DrainOnce();
				ReportDrops();

				const Tick now = Platform::GetTick();
				const bool flushRequested = request > s_FlushComplete.load(std::memory_order_relaxed);
				const bool flushDue = drained > 0
					&& Platform::ToMicroseconds(lastFlush, now) >= s_FlushIntervalMicroseconds;

				if (flushRequested || flushDue || !running)
				{
					for (const Scoped<LogSink>& sink : Sinks())
						sink->Flush();

					lastFlush = now;
					s_FlushComplete.store(request, std::memory_order_release);
				}

				if (!running)
					break;

				if (drained > 0)
				{
					idleSpins = 0;
					continue;
				}

				// Polling rather than a condition variable keeps the push path free of any notify, and
				// logging tolerates the latency: a millisecond between call site and console is invisible.
				if (++idleSpins < 64)
					CDL_THREAD_PAUSE();
				else
					std::this_thread::sleep_for(std::chrono::milliseconds(idleSpins < 1024 ? 1 : 8));
			}
		}

	}

	const char* LogLevelName(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::Trace: return "TRACE";
		case LogLevel::Info:  return "INFO";
		case LogLevel::Warn:  return "WARN";
		case LogLevel::Error: return "ERROR";
		case LogLevel::Fatal: return "FATAL";
		default:              return "INFO";
		}
	}

	const char* LogChannelName(LogChannel channel)
	{
		static constexpr const char* s_Names[] = {
			"App", "AI", "Animation", "Render", "Input", "Audio", "Physics", "Memory", "Thread", "Math"
		};

		const uint16_t value = static_cast<uint16_t>(channel);
		if (value == 0)
			return "None";

		const size_t index = static_cast<size_t>(std::countr_zero(value));
		return index < std::size(s_Names) ? s_Names[index] : "?";
	}

	const char* LoggerTypeName(uint8_t type)
	{
		return type == 0 ? "ENGINE" : "APP";
	}

	void Logger::Init(const LoggerSpecification& spec)
	{
		CDL_CORE_ASSERT(!s_Running.load(std::memory_order_relaxed), "Logger::Init called twice");

		s_Level.store(spec.Level, std::memory_order_relaxed);
		s_ChannelMask.store(spec.ChannelMask, std::memory_order_relaxed);

		if (spec.LogToConsole)
			AddSink(std::make_unique<ConsoleSink>());

		if (spec.LogToFile)
			AddSink(std::make_unique<FileSink>(spec.LogFilePath));

		s_Running.store(true, std::memory_order_release);
		s_DrainThread = std::thread(DrainThreadMain);
	}

	void Logger::Shutdown()
	{
		if (!s_Running.exchange(false, std::memory_order_acq_rel))
			return;

		if (s_DrainThread.joinable())
			s_DrainThread.join();

		// Anything pushed between the drain thread's final pass and the join. Safe on this thread only
		// because the drain thread is gone, so there is no second consumer.
		DrainOnce();

		for (const Scoped<LogSink>& sink : Sinks())
			sink->Flush();

		Sinks().clear();
	}

	void Logger::AddSink(Scoped<LogSink> sink)
	{
		CDL_CORE_ASSERT(!s_Running.load(std::memory_order_relaxed),
			"Sinks must be added before Logger::Init -- the drain thread owns them unsynchronised");

		if (sink)
			Sinks().push_back(std::move(sink));
	}

	bool Logger::Flush()
	{
		// Nothing will ever satisfy the handshake without a drain thread to answer it.
		if (!s_Running.load(std::memory_order_acquire))
			return false;

		const uint64_t ticket = s_FlushRequest.fetch_add(1, std::memory_order_release) + 1;
		const Tick start = Platform::GetTick();

		while (s_FlushComplete.load(std::memory_order_acquire) < ticket)
		{
			if (Platform::ToMicroseconds(start, Platform::GetTick()) > s_FlushTimeoutMicroseconds)
				return false;

			CDL_THREAD_PAUSE();
		}

		return true;
	}

	void Logger::PushRecord(const LogRecord& record)
	{
		bool pushSucceeded = Ring().TryPush(record);

		// If the ring is full, the record is dropped. If it's a fatal record, we must write it to stderr immediately.
		if (!pushSucceeded && record.Level == LogLevel::Fatal)
			EmergencyWrite(record);
	}

	void Logger::EmergencyWrite(const LogRecord& record)
	{
		std::fprintf(stderr, "[%s] [%s] (T%u) %s\n",
			LoggerTypeName(record.Type),
			LogLevelName(record.Level),
			record.ThreadId,
			record.Message);
		std::fflush(stderr);
	}

}
