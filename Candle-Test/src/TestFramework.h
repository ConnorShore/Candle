#pragma once

// Small, dependency-free test framework, modelled on Ember-Test's. It does not depend on the engine.
//
// CDL_CHECK* throws and aborts the current test; CDL_EXPECT* records the failure and continues.
// RunAll() returns the failed-test count so the runner can use it as the process exit code.
//
// Threading contract: every CDL_CHECK / CDL_EXPECT / CDL_SKIP / CDL_NOTE must run on the thread
// executing the test body. A CHECK throwing on a worker std::thread calls std::terminate, and the
// soft-failure and note lists are unsynchronised. Multi-threaded tests record what each worker saw,
// join, and only then assert.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <format>
#include <print>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Candle::Test {

	// Run-time filterable, so the slow multi-threaded hammering can be skipped with --filter=unit.
	namespace Type {
		inline constexpr const char* Unit = "unit";     // single-threaded logic, fast
		inline constexpr const char* Stress = "stress"; // many threads or many iterations, hunting races
	}

	// Thrown by CDL_CHECK* to abort the current test.
	struct TestFailure { std::string Message; };
	// Thrown by CDL_SKIP to mark a test as skipped rather than failed.
	struct TestSkipped { std::string Reason; };

	struct TestCase
	{
		const char* Suite;
		const char* Name;
		const char* TestType;
		void (*Fn)();
	};

	// Function-local static so registration from any translation unit's static initialiser is safe.
	class Registry
	{
	public:
		static Registry& Get() { static Registry s_Instance; return s_Instance; }
		void Add(const TestCase& test) { m_Tests.push_back(test); }
		const std::vector<TestCase>& Tests() const { return m_Tests; }
	private:
		std::vector<TestCase> m_Tests;
	};

	struct AutoRegister
	{
		AutoRegister(const char* suite, const char* name, const char* type, void (*fn)())
		{
			Registry::Get().Add(TestCase{ suite, name, type, fn });
		}
	};

	// Per-test scratch state, cleared before each test runs. Test-body thread only.
	class Context
	{
	public:
		static Context& Get() { static Context s_Instance; return s_Instance; }

		void Begin() { SoftFailures.clear(); Notes.clear(); }

		std::vector<std::string> SoftFailures;
		std::vector<std::string> Notes; // printed under the test line on pass or fail
	};

	//////////////////////////////////////////////////////////////////////////
	// Failure reporting
	//////////////////////////////////////////////////////////////////////////

	inline std::string FormatFailure(const char* expr, const char* file, int line, const std::string& extra)
	{
		// Filename only; full paths make terminal output unreadable.
		const char* slash = std::strrchr(file, '\\');
		if (!slash) slash = std::strrchr(file, '/');
		const char* shortName = slash ? slash + 1 : file;

		std::string message = std::format("{}({}): {}", shortName, line, expr);
		if (!extra.empty())
			message += "  -> " + extra;
		return message;
	}

	[[noreturn]] inline void ReportFail(const char* expr, const char* file, int line, const std::string& extra = {})
	{
		throw TestFailure{ FormatFailure(expr, file, line, extra) };
	}

	inline void ReportSoftFail(const char* expr, const char* file, int line, const std::string& extra = {})
	{
		Context::Get().SoftFailures.push_back(FormatFailure(expr, file, line, extra));
	}

	// Best-effort printing of a compared value, so a failed EQ says what the values actually were.
	template<typename T>
	std::string Describe(const T& value)
	{
		if constexpr (std::is_enum_v<T>)
			return std::format("{}", std::to_underlying(value));
		else if constexpr (std::is_pointer_v<T>)
			return std::format("{}", static_cast<const void*>(value));
		else if constexpr (std::formattable<T, char>)
			return std::format("{}", value);
		else
			return "<unprintable>";
	}

	template<typename A, typename B>
	std::string DescribePair(const A& a, const B& b)
	{
		return Describe(a) + " vs " + Describe(b);
	}

	//////////////////////////////////////////////////////////////////////////
	// Benchmarking
	//////////////////////////////////////////////////////////////////////////

	struct BenchmarkResult
	{
		double MinMs = 0.0;
		double MedianMs = 0.0;
		double MeanMs = 0.0;
		double MaxMs = 0.0;
		int Iterations = 0;

		std::string ToString() const
		{
			return std::format("median {:.4f} ms (min {:.4f}, mean {:.4f}, max {:.4f}) over {} iters",
				MedianMs, MinMs, MeanMs, MaxMs, Iterations);
		}
	};

	// The median is the headline because, unlike the mean, it shrugs off an occasional OS scheduling hiccup.
	template<typename Fn>
	BenchmarkResult Benchmark(Fn&& fn, int iterations, int warmup = 3)
	{
		for (int i = 0; i < warmup; ++i)
			fn();

		std::vector<double> samples;
		samples.reserve(iterations);
		for (int i = 0; i < iterations; ++i)
		{
			const auto start = std::chrono::steady_clock::now();
			fn();
			const auto end = std::chrono::steady_clock::now();
			samples.push_back(std::chrono::duration<double, std::milli>(end - start).count());
		}

		std::ranges::sort(samples);

		BenchmarkResult result;
		result.Iterations = iterations;
		result.MinMs = samples.front();
		result.MaxMs = samples.back();
		result.MedianMs = samples[samples.size() / 2];
		for (double sample : samples)
			result.MeanMs += sample;
		result.MeanMs /= static_cast<double>(samples.size());
		return result;
	}

	// Budgets are hardware-dependent; CDL_TEST_PERF_SCALE=4 lets a slow box or a Debug build use the same numbers.
	inline double PerfScale()
	{
		static const double s_Scale = [] {
			const char* raw = std::getenv("CDL_TEST_PERF_SCALE");
			const double parsed = raw ? std::atof(raw) : 1.0;
			return parsed > 0.0 ? parsed : 1.0;
		}();
		return s_Scale;
	}

	//////////////////////////////////////////////////////////////////////////
	// Runner
	//////////////////////////////////////////////////////////////////////////

	struct RunOptions
	{
		std::string TypeFilter; // exact match against TestCase::TestType, empty for all
		std::string NameFilter; // substring match against "Suite::Name", empty for all
	};

	inline bool Matches(const TestCase& test, const RunOptions& options)
	{
		if (!options.TypeFilter.empty() && options.TypeFilter != test.TestType)
			return false;

		const std::string fullName = std::format("{}::{}", test.Suite, test.Name);
		return options.NameFilter.empty() || fullName.find(options.NameFilter) != std::string::npos;
	}

	inline int RunAll(const RunOptions& options = {})
	{
		struct SuiteTally { std::string Name; int Passed = 0, Failed = 0, Skipped = 0; double Ms = 0.0; };
		std::vector<SuiteTally> suites;
		int passed = 0, failed = 0, skipped = 0, filteredOut = 0;

		std::println("\n================ Candle-Test ================");
		if (!options.TypeFilter.empty()) std::println("  type filter : {}", options.TypeFilter);
		if (!options.NameFilter.empty()) std::println("  name filter : {}", options.NameFilter);
		std::println("---------------------------------------------");

		const auto runStart = std::chrono::steady_clock::now();

		for (const TestCase& test : Registry::Get().Tests())
		{
			if (!Matches(test, options))
			{
				++filteredOut;
				continue;
			}

			Context& context = Context::Get();
			context.Begin();

			std::vector<std::string> failures;
			std::string skipReason;
			bool wasSkipped = false;

			const auto start = std::chrono::steady_clock::now();
			try
			{
				test.Fn();
			}
			catch (const TestFailure& failure) { failures.push_back(failure.Message); }
			catch (const TestSkipped& skip)    { skipReason = skip.Reason; wasSkipped = true; }
			catch (const std::exception& e)    { failures.push_back(std::format("unexpected exception: {}", e.what())); }
			catch (...)                        { failures.push_back("unknown exception thrown"); }
			const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

			// Soft failures come first because they happened before whatever hard failure ended the test.
			if (!wasSkipped)
				failures.insert(failures.begin(), context.SoftFailures.begin(), context.SoftFailures.end());

			const char* tag = wasSkipped ? "SKIP" : failures.empty() ? "PASS" : "FAIL";
			std::println("  [{}] [{:<6}] {:<60} {:8.2f} ms", tag, test.TestType,
				std::format("{}::{}", test.Suite, test.Name), ms);

			for (const std::string& note : context.Notes)
				std::println("         . {}", note);
			for (const std::string& failure : failures)
				std::println("         ! {}", failure);
			if (wasSkipped)
				std::println("         ~ {}", skipReason);

			auto suite = std::ranges::find(suites, std::string(test.Suite), &SuiteTally::Name);
			if (suite == suites.end())
				suite = suites.insert(suites.end(), SuiteTally{ test.Suite });

			suite->Ms += ms;
			if (wasSkipped)            { ++skipped; ++suite->Skipped; }
			else if (failures.empty()) { ++passed;  ++suite->Passed; }
			else                       { ++failed;  ++suite->Failed; }

			// Flushed per test so the last line on screen names the culprit if the next test hard-crashes.
			std::fflush(stdout);
		}

		const double totalMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - runStart).count();

		std::println("---------------------------------------------");
		for (const SuiteTally& suite : suites)
			std::println("  {:<22} {:3} passed  {:3} failed  {:3} skipped  {:8.2f} ms",
				suite.Name, suite.Passed, suite.Failed, suite.Skipped, suite.Ms);
		std::println("---------------------------------------------");
		std::print("  {} passed, {} failed, {} skipped", passed, failed, skipped);
		if (filteredOut)
			std::print(", {} filtered out", filteredOut);
		std::println("   [{:.2f} ms total]", totalMs);
		std::println("  RESULT: {}", failed == 0 ? "OK" : "FAILURES");
		std::println("=============================================\n");
		std::fflush(stdout);

		return failed;
	}

	inline void ListTests()
	{
		for (const TestCase& test : Registry::Get().Tests())
			std::println("  [{:<6}] {}::{}", test.TestType, test.Suite, test.Name);
		std::println("  {} tests registered", Registry::Get().Tests().size());
	}

} // namespace Candle::Test

// Defines a test body and self-registers it. TestType is one of Candle::Test::Type::*.
#define CDL_TEST_CASE(Suite, Name, TestType)                                                                  \
	static void Suite##_##Name##_Body();                                                                      \
	static ::Candle::Test::AutoRegister Suite##_##Name##_Registrar(#Suite, #Name, (TestType), &Suite##_##Name##_Body); \
	static void Suite##_##Name##_Body()

// Shared body for every comparison macro. Each operand is evaluated exactly once.
#define CDL_TEST_COMPARE_IMPL(reportFn, macroName, a, b, op)                                                  \
	do {                                                                                                      \
		const auto& _cdlA = (a);                                                                              \
		const auto& _cdlB = (b);                                                                              \
		if (!(_cdlA op _cdlB))                                                                                \
			reportFn(macroName "(" #a ", " #b ")", __FILE__, __LINE__, ::Candle::Test::DescribePair(_cdlA, _cdlB)); \
	} while (0)

#define CDL_TEST_NEAR_IMPL(reportFn, macroName, a, b, eps)                                                    \
	do {                                                                                                      \
		const double _cdlA = static_cast<double>(a), _cdlB = static_cast<double>(b);                          \
		if (!(std::abs(_cdlA - _cdlB) <= static_cast<double>(eps)))                                           \
			reportFn(macroName "(" #a ", " #b ", " #eps ")", __FILE__, __LINE__,                              \
				std::format("{} vs {} (delta {})", _cdlA, _cdlB, std::abs(_cdlA - _cdlB)));                   \
	} while (0)

//////////////////////////////////////////////////////////////////////////
// Hard assertions - abort the current test; the runner moves on to the next one.
//////////////////////////////////////////////////////////////////////////

#define CDL_CHECK(cond)           do { if (!(cond)) ::Candle::Test::ReportFail("CHECK(" #cond ")", __FILE__, __LINE__); } while (0)
#define CDL_CHECK_FALSE(cond)     do { if ( (cond)) ::Candle::Test::ReportFail("CHECK_FALSE(" #cond ")", __FILE__, __LINE__); } while (0)
#define CDL_CHECK_MSG(cond, msg)  do { if (!(cond)) ::Candle::Test::ReportFail("CHECK(" #cond ")", __FILE__, __LINE__, (msg)); } while (0)
#define CDL_CHECK_EQ(a, b)        CDL_TEST_COMPARE_IMPL(::Candle::Test::ReportFail, "CHECK_EQ", a, b, ==)
#define CDL_CHECK_NE(a, b)        CDL_TEST_COMPARE_IMPL(::Candle::Test::ReportFail, "CHECK_NE", a, b, !=)
#define CDL_CHECK_NEAR(a, b, eps) CDL_TEST_NEAR_IMPL(::Candle::Test::ReportFail, "CHECK_NEAR", a, b, eps)

//////////////////////////////////////////////////////////////////////////
// Soft assertions - record the failure and keep going, so one run reports every wrong value.
//////////////////////////////////////////////////////////////////////////

#define CDL_EXPECT(cond)           do { if (!(cond)) ::Candle::Test::ReportSoftFail("EXPECT(" #cond ")", __FILE__, __LINE__); } while (0)
#define CDL_EXPECT_FALSE(cond)     do { if ( (cond)) ::Candle::Test::ReportSoftFail("EXPECT_FALSE(" #cond ")", __FILE__, __LINE__); } while (0)
#define CDL_EXPECT_MSG(cond, msg)  do { if (!(cond)) ::Candle::Test::ReportSoftFail("EXPECT(" #cond ")", __FILE__, __LINE__, (msg)); } while (0)
#define CDL_EXPECT_EQ(a, b)        CDL_TEST_COMPARE_IMPL(::Candle::Test::ReportSoftFail, "EXPECT_EQ", a, b, ==)
#define CDL_EXPECT_NE(a, b)        CDL_TEST_COMPARE_IMPL(::Candle::Test::ReportSoftFail, "EXPECT_NE", a, b, !=)
#define CDL_EXPECT_GT(a, b)        CDL_TEST_COMPARE_IMPL(::Candle::Test::ReportSoftFail, "EXPECT_GT", a, b, >)
#define CDL_EXPECT_GE(a, b)        CDL_TEST_COMPARE_IMPL(::Candle::Test::ReportSoftFail, "EXPECT_GE", a, b, >=)
#define CDL_EXPECT_LT(a, b)        CDL_TEST_COMPARE_IMPL(::Candle::Test::ReportSoftFail, "EXPECT_LT", a, b, <)
#define CDL_EXPECT_LE(a, b)        CDL_TEST_COMPARE_IMPL(::Candle::Test::ReportSoftFail, "EXPECT_LE", a, b, <=)
#define CDL_EXPECT_NEAR(a, b, eps) CDL_TEST_NEAR_IMPL(::Candle::Test::ReportSoftFail, "EXPECT_NEAR", a, b, eps)

//////////////////////////////////////////////////////////////////////////
// Reporting helpers
//////////////////////////////////////////////////////////////////////////

// Marks the test as skipped, not failed, and stops it. For fixtures that may be missing on some machines.
#define CDL_SKIP(reason) throw ::Candle::Test::TestSkipped{ (reason) }

// Printed under the test line on pass and fail, for numbers a human wants to see either way.
#define CDL_NOTE(msg)    ::Candle::Test::Context::Get().Notes.push_back((msg))

//////////////////////////////////////////////////////////////////////////
// Benchmarking - the block is __VA_ARGS__ so commas inside it don't split the macro arguments.
//   CDL_BENCH_BUDGET("push 1k", 0.5, 50, { for (...) ring.TryPush(x); });
//////////////////////////////////////////////////////////////////////////

// Soft-fails if the median exceeds budgetMs * CDL_TEST_PERF_SCALE.
#define CDL_BENCH_BUDGET(label, budgetMs, iterations, ...)                                                   \
	do {                                                                                                     \
		const ::Candle::Test::BenchmarkResult _cdlResult = ::Candle::Test::Benchmark([&]() __VA_ARGS__, (iterations)); \
		const double _cdlBudget = static_cast<double>(budgetMs) * ::Candle::Test::PerfScale();               \
		CDL_NOTE(std::format("{}: {}  [budget {:.4f} ms]", (label), _cdlResult.ToString(), _cdlBudget));      \
		CDL_EXPECT_MSG(_cdlResult.MedianMs <= _cdlBudget,                                                    \
			std::format("{} median {:.4f} ms exceeded budget {:.4f} ms", (label), _cdlResult.MedianMs, _cdlBudget)); \
	} while (0)

// Measurement only, for establishing a baseline or for numbers too noisy to gate on.
#define CDL_BENCH_REPORT(label, iterations, ...)                                                             \
	do {                                                                                                     \
		const ::Candle::Test::BenchmarkResult _cdlResult = ::Candle::Test::Benchmark([&]() __VA_ARGS__, (iterations)); \
		CDL_NOTE(std::format("{}: {}", (label), _cdlResult.ToString()));                                      \
	} while (0)
