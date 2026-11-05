// ConsoleArgs parsed directly: the line a command gets, after ConsoleManager has taken its name off the front.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Debug/ConsoleArgs.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	int CountWarnings(const std::vector<LogRecord>& records, std::string_view prefix)
	{
		return static_cast<int>(std::ranges::count_if(records, [prefix](const LogRecord& record) {
			return record.Level == LogLevel::Warn && record.Channel == LogChannel::Command && std::string_view(record.Message).starts_with(prefix);
			}));
	}

}

CDL_TEST_CASE(ConsoleArgs, BlankLinesHaveNoArgs, Unit)
{
	for (const std::string_view line : { "", " ", "\t", " \t  " })
		CDL_EXPECT_EQ(ConsoleArgs(line).GetCount(), 0u);
}

CDL_TEST_CASE(ConsoleArgs, SplitsOnAnyRunOfSpacesAndTabs, Unit)
{
	const ConsoleArgs args("  one two\t\tthree \t");

	CDL_CHECK_EQ(args.GetCount(), 3u);
	CDL_EXPECT(args.TryGetString(0) == "one");
	CDL_EXPECT(args.TryGetString(1) == "two");
	CDL_EXPECT(args.TryGetString(2) == "three");
}

CDL_TEST_CASE(ConsoleArgs, QuotedArgKeepsItsWhitespaceAndLosesItsQuotes, Unit)
{
	ConsoleArgs args("plain \"two  words\"");

	CDL_CHECK_EQ(args.GetCount(), 2u);
	CDL_EXPECT(args.TryGetString(1) == "two  words");
	CDL_EXPECT_FALSE(args.GetAll()[0].IsQuoted);
	CDL_EXPECT(args.GetAll()[1].IsQuoted);
}

CDL_TEST_CASE(ConsoleArgs, EmptyQuotesAreAnEmptyArg, Unit)
{
	const ConsoleArgs args("one \"\" two");

	CDL_CHECK_EQ(args.GetCount(), 3u);
	CDL_EXPECT(args.TryGetString(1) == "");
	CDL_EXPECT(args.TryGetString(2) == "two");
}

CDL_TEST_CASE(ConsoleArgs, ClosingQuoteEndsAnArgWithoutASpace, Unit)
{
	const ConsoleArgs args("\"one\"two");

	CDL_CHECK_EQ(args.GetCount(), 2u);
	CDL_EXPECT(args.TryGetString(0) == "one");
	CDL_EXPECT(args.TryGetString(1) == "two");
}

CDL_TEST_CASE(ConsoleArgs, UnmatchedQuoteKeepsTheArgsBeforeItAndWarns, Unit)
{
	LoggerFixture logger;

	const ConsoleArgs args("one two \"never closed");
	CDL_CHECK(Logger::Flush());

	CDL_CHECK_EQ(args.GetCount(), 2u);
	CDL_EXPECT(args.TryGetString(1) == "two");
	CDL_EXPECT_EQ(CountWarnings(logger.Records, "Unmatched quote"), 1);
}

// What autocomplete's replace range will be built from, so the last token has to end at the line's end too
CDL_TEST_CASE(ConsoleArgs, TokensRecordWhereTheySitInTheLine, Unit)
{
	ConsoleArgs args("one  two");

	CDL_CHECK_EQ(args.GetCount(), 2u);
	CDL_EXPECT_EQ(args.GetAll()[0].StartIndex, 0u);
	CDL_EXPECT_EQ(args.GetAll()[0].EndIndex, 3u);
	CDL_EXPECT_EQ(args.GetAll()[1].StartIndex, 5u);
	CDL_EXPECT_EQ(args.GetAll()[1].EndIndex, 8u);
}

CDL_TEST_CASE(ConsoleArgs, MatchesIgnoresCase, Unit)
{
	const ConsoleArgs args("Verbose \"Two Words\"");

	CDL_EXPECT(args.Matches(0, "verbose"));
	CDL_EXPECT(args.Matches(0, "VERBOSE"));
	CDL_EXPECT(args.Matches(1, "two words"));
	CDL_EXPECT_FALSE(args.Matches(0, "verb"));
}

CDL_TEST_CASE(ConsoleArgs, LookupsPastTheEndComeBackEmpty, Unit)
{
	const ConsoleArgs args("one");

	CDL_EXPECT_FALSE(args.Matches(1, "one"));
	CDL_EXPECT_FALSE(args.TryGetString(1).has_value());
	CDL_EXPECT_FALSE(args.TryGet<std::string>(1).has_value());
	CDL_EXPECT_FALSE(args.TryGet<int>(1).has_value());
}

CDL_TEST_CASE(ConsoleArgs, TryGetConvertsToTheTypeAskedFor, Unit)
{
	const ConsoleArgs args("42 -7 2.5 text");

	CDL_EXPECT_EQ(args.TryGet<int>(0).value_or(0), 42);
	CDL_EXPECT_EQ(args.TryGet<int64_t>(1).value_or(0), -7);
	CDL_EXPECT_EQ(args.TryGet<float>(2).value_or(0.0f), 2.5f);
	CDL_EXPECT_EQ(args.TryGet<double>(2).value_or(0.0), 2.5);
	CDL_EXPECT_EQ(args.TryGet<std::string>(3).value_or(""), "text");
}

CDL_TEST_CASE(ConsoleArgs, WholeNumbersAreValidFloats, Unit)
{
	const ConsoleArgs args("3");

	CDL_EXPECT_EQ(args.TryGet<float>(0).value_or(0.0f), 3.0f);
}

CDL_TEST_CASE(ConsoleArgs, ArgThatCannotConvertComesBackEmptyAndWarns, Unit)
{
	LoggerFixture logger;
	const ConsoleArgs args("text 99999999999999999999");

	CDL_EXPECT_FALSE(args.TryGet<int>(0).has_value());
	CDL_EXPECT_FALSE(args.TryGet<float>(0).has_value());
	CDL_EXPECT_FALSE(args.TryGet<int64_t>(1).has_value());	// Too big for stoll, which throws rather than saturating
	CDL_CHECK(Logger::Flush());

	CDL_EXPECT_EQ(CountWarnings(logger.Records, "Failed to convert argument 0"), 2);
	CDL_EXPECT_EQ(CountWarnings(logger.Records, "Failed to convert argument 1"), 1);
}
