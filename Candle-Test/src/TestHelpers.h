#pragma once

// Engine-aware helpers shared by the tests. TestFramework.h stays engine-free; anything that needs
// Candle types lives here.

#include <Candle.h>
#include <Candle/Core/LogSink.h>

#include <latch>
#include <thread>
#include <vector>

namespace Candle::Test {

	// Counts constructions and destructions so ownership tests can assert "deleted exactly once".
	// Static counters, so every test must call Reset() first. Test-body thread only.
	struct LifetimeCounter
	{
		inline static int s_Constructed = 0;
		inline static int s_Destroyed = 0;
		static void Reset() { s_Constructed = 0; s_Destroyed = 0; }
		static int Alive() { return s_Constructed - s_Destroyed; }

		explicit LifetimeCounter(int value = 0) : Value(value) { ++s_Constructed; }
		virtual ~LifetimeCounter() { ++s_Destroyed; }

		int Value;
	};

	struct DerivedLifetimeCounter : LifetimeCounter
	{
		inline static int s_DerivedDestroyed = 0;
		using LifetimeCounter::LifetimeCounter;
		~DerivedLifetimeCounter() override { ++s_DerivedDestroyed; }
	};

	// The SharedPtr flavour. The destroy counter is atomic because the last release, and therefore
	// the delete, can happen on any thread in the stress tests.
	struct SharedCounter : SharedResource
	{
		inline static std::atomic<int> s_Destroyed = 0;
		static void Reset() { s_Destroyed = 0; }

		explicit SharedCounter(int value = 0) : Value(value) {}
		~SharedCounter() override { s_Destroyed.fetch_add(1, std::memory_order_relaxed); }

		int Value;
	};

	struct DerivedSharedCounter : SharedCounter
	{
		using SharedCounter::SharedCounter;
	};

	struct UnrelatedSharedCounter : SharedCounter
	{
		using SharedCounter::SharedCounter;
	};

	// Collects every record the drain thread hands it into a vector the caller owns.
	// Threading: Write runs on the drain thread. The caller may read the vector only after a
	// successful Logger::Flush() (its handshake orders the writes before the read) or after Shutdown.
	class CaptureSink : public LogSink
	{
	public:
		explicit CaptureSink(std::vector<LogRecord>& out) : m_Out(out) {}

		void Write(const LogRecord* records, size_t count) override { m_Out.insert(m_Out.end(), records, records + count); }
		void Flush() override {}

	private:
		std::vector<LogRecord>& m_Out;
	};

	// Owns one Logger Init/Shutdown cycle with a CaptureSink and no console output. Restores the
	// default level and mask afterwards, because both are process-wide statics.
	class LoggerFixture
	{
	public:
		explicit LoggerFixture(LogLevel level = LogLevel::Trace, uint16_t channelMask = 0xFFFF)
		{
			Logger::AddSink(ScopedPtr<CaptureSink>::Create(Records));

			LoggerSpecification spec;
			spec.Level = level;
			spec.ChannelMask = channelMask;
			spec.LogToConsole = false;
			spec.LogToFile = false;
			Logger::Init(spec);
		}

		~LoggerFixture()
		{
			Logger::Shutdown();
			Logger::SetLevel(LogLevel::Info);
			Logger::SetChannelMask(0xFFFF);
		}

		LoggerFixture(const LoggerFixture&) = delete;
		LoggerFixture& operator=(const LoggerFixture&) = delete;

		// Read only after Logger::Flush() returned true, or after this fixture is destroyed.
		std::vector<LogRecord> Records;
	};

	// Starts threadCount threads that all wait on a latch and then call fn(threadIndex) together,
	// which maximises contention. Returns once every thread has joined.
	template<typename Fn>
	void RunConcurrently(int threadCount, Fn&& fn)
	{
		std::latch start(threadCount);
		std::vector<std::jthread> threads;
		threads.reserve(threadCount);

		for (int i = 0; i < threadCount; ++i)
			threads.emplace_back([&, i] { start.arrive_and_wait(); fn(i); });
	}

	// Enough threads to contend on any desktop, without oversubscribing a small CI box too badly.
	inline int StressThreadCount()
	{
		const unsigned hardware = std::thread::hardware_concurrency();
		return static_cast<int>(std::clamp(hardware, 4u, 16u));
	}

}
