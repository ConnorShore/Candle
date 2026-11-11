// Platform's file calls: byte-exact round trips, the text read's byte-order-mark handling, directory creation, error codes
// a caller can branch on, and concurrent use. Sharing violations need a raw OS handle, so they live under Platform/Windows.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using namespace std::string_view_literals;
using Candle::Test::Type::Stress;
using Candle::Test::Type::Unit;

namespace {

	std::span<const std::byte> AsBytes(std::string_view text) { return std::as_bytes(std::span(text)); }

	std::vector<std::byte> ToBytes(std::string_view text)
	{
		const std::span<const std::byte> bytes = AsBytes(text);
		return { bytes.begin(), bytes.end() };
	}

	// Distinct per index, so a reader that got another thread's file would notice.
	std::vector<std::byte> Pattern(size_t size, uint32_t seed)
	{
		std::vector<std::byte> bytes(size);
		for (size_t i = 0; i < size; ++i)
			bytes[i] = static_cast<std::byte>((i * 31 + seed * 17) & 0xFF);
		return bytes;
	}

}

// Every byte value, plus the ones a C runtime text mode rewrites: CR LF pairs, Ctrl-Z and NUL.
CDL_TEST_CASE(FileSystem, RoundTripsEveryByteValueExactly, Unit)
{
	TempDirectory dir;
	std::vector<std::byte> written;
	for (int value = 0; value < 256; ++value)
		written.push_back(static_cast<std::byte>(value));
	const std::vector<std::byte> textModeHazards = ToBytes("\r\n\x1A\0\r\n"sv);
	written.insert(written.end(), textModeHazards.begin(), textModeHazards.end());

	CDL_CHECK(Platform::WriteFile(dir / "bytes.bin", written).has_value());

	const auto read = Platform::ReadFile(dir / "bytes.bin");
	CDL_CHECK(read.has_value());
	CDL_CHECK_EQ(read->size(), written.size());
	CDL_EXPECT(*read == written);
}

CDL_TEST_CASE(FileSystem, RoundTripsAnEmptyFile, Unit)
{
	TempDirectory dir;
	CDL_CHECK(Platform::WriteFile(dir / "empty.bin", {}).has_value());

	const auto bytes = Platform::ReadFile(dir / "empty.bin");
	CDL_CHECK(bytes.has_value());
	CDL_EXPECT(bytes->empty());

	const auto text = Platform::ReadTextFile(dir / "empty.bin");
	CDL_CHECK(text.has_value());
	CDL_EXPECT(text->empty());
}

// CREATE_ALWAYS truncates, so a shorter write must not leave the old file's tail behind.
CDL_TEST_CASE(FileSystem, WriteReplacesAnExistingFile, Unit)
{
	TempDirectory dir;
	CDL_CHECK(Platform::WriteFile(dir / "file.bin", Pattern(4096, 1)).has_value());
	CDL_CHECK(Platform::WriteFile(dir / "file.bin", Pattern(16, 2)).has_value());

	const auto read = Platform::ReadFile(dir / "file.bin");
	CDL_CHECK(read.has_value());
	CDL_EXPECT(*read == Pattern(16, 2));
}

// The span signature exists so non-byte containers, e.g. SPIR-V words, are written without a copy into a byte vector.
CDL_TEST_CASE(FileSystem, WritesAnyContiguousContainer, Unit)
{
	TempDirectory dir;
	const std::vector<uint32_t> words = { 0x07230203u, 0x00010600u, 0xDEADBEEFu, 0u };
	CDL_CHECK(Platform::WriteFile(dir / "words.bin", std::as_bytes(std::span(words))).has_value());

	const auto read = Platform::ReadFile(dir / "words.bin");
	CDL_CHECK(read.has_value());
	CDL_CHECK_EQ(read->size(), words.size() * sizeof(uint32_t));
	CDL_EXPECT(std::memcmp(read->data(), words.data(), read->size()) == 0);
}

// Cyrillic and kana are in no single ANSI code page, so this fails if a narrow-string API sneaks into the path handling.
CDL_TEST_CASE(FileSystem, HandlesPathsOutsideTheAnsiCodePage, Unit)
{
	TempDirectory dir;
	const std::filesystem::path path = dir / L"файл-ファイル.bin";
	CDL_CHECK(Platform::WriteFile(path, Pattern(64, 3)).has_value());

	const auto read = Platform::ReadFile(path);
	CDL_CHECK(read.has_value());
	CDL_EXPECT(*read == Pattern(64, 3));
}

// The text read is the binary read into a std::string: no CRLF collapse, no Ctrl-Z end of file, NULs kept.
CDL_TEST_CASE(FileSystem, ReadTextFileKeepsTheBytesVerbatim, Unit)
{
	TempDirectory dir;
	constexpr std::string_view content = "one\r\ntwo\nthree\x1A" "four\0five"sv;
	CDL_CHECK(Platform::WriteFile(dir / "text.txt", AsBytes(content)).has_value());

	const auto text = Platform::ReadTextFile(dir / "text.txt");
	CDL_CHECK(text.has_value());
	CDL_CHECK_EQ(text->size(), content.size());
	CDL_EXPECT(*text == content);
}

CDL_TEST_CASE(FileSystem, ReadTextFileStripsALeadingByteOrderMark, Unit)
{
	TempDirectory dir;
	CDL_CHECK(Platform::WriteFile(dir / "bom.txt", AsBytes("\xEF\xBB\xBFhello"sv)).has_value());
	CDL_CHECK(Platform::WriteFile(dir / "only-bom.txt", AsBytes("\xEF\xBB\xBF"sv)).has_value());

	const auto text = Platform::ReadTextFile(dir / "bom.txt");
	CDL_CHECK(text.has_value());
	CDL_EXPECT_EQ(*text, std::string("hello"));

	const auto onlyBom = Platform::ReadTextFile(dir / "only-bom.txt");
	CDL_CHECK(onlyBom.has_value());
	CDL_EXPECT(onlyBom->empty());

	// The binary read is verbatim, mark included.
	const auto bytes = Platform::ReadFile(dir / "bom.txt");
	CDL_CHECK(bytes.has_value());
	CDL_EXPECT(*bytes == ToBytes("\xEF\xBB\xBFhello"sv));
}

// Only a complete mark at offset 0 is an encoding marker; anywhere else, or cut short, it is content.
CDL_TEST_CASE(FileSystem, ReadTextFileKeepsAByteOrderMarkThatIsNotLeading, Unit)
{
	TempDirectory dir;
	CDL_CHECK(Platform::WriteFile(dir / "middle.txt", AsBytes("hi\xEF\xBB\xBF"sv)).has_value());
	CDL_CHECK(Platform::WriteFile(dir / "partial.txt", AsBytes("\xEF\xBBx"sv)).has_value());

	const auto middle = Platform::ReadTextFile(dir / "middle.txt");
	CDL_CHECK(middle.has_value());
	CDL_EXPECT_EQ(*middle, std::string("hi\xEF\xBB\xBF"));

	const auto partial = Platform::ReadTextFile(dir / "partial.txt");
	CDL_CHECK(partial.has_value());
	CDL_EXPECT_EQ(*partial, std::string("\xEF\xBBx"));
}

// A missing file and a missing parent directory are different OS errors, but the same thing to a caller.
CDL_TEST_CASE(FileSystem, MissingFileOrDirectoryIsNotFound, Unit)
{
	TempDirectory dir;

	const auto missingFile = Platform::ReadFile(dir / "missing.bin");
	CDL_CHECK_FALSE(missingFile.has_value());
	CDL_EXPECT_EQ(missingFile.error().Code, FileErrorCode::NotFound);
	CDL_EXPECT_NE(missingFile.error().OSError, 0u);

	const auto missingText = Platform::ReadTextFile(dir / "missing.txt");
	CDL_CHECK_FALSE(missingText.has_value());
	CDL_EXPECT_EQ(missingText.error().Code, FileErrorCode::NotFound);

	const auto missingDirectory = Platform::ReadFile(dir / "no-such-dir" / "file.bin");
	CDL_CHECK_FALSE(missingDirectory.has_value());
	CDL_EXPECT_EQ(missingDirectory.error().Code, FileErrorCode::NotFound);

	// WriteFile creates the file but never its parent directories.
	const auto written = Platform::WriteFile(dir / "no-such-dir" / "file.bin", Pattern(8, 4));
	CDL_CHECK_FALSE(written.has_value());
	CDL_EXPECT_EQ(written.error().Code, FileErrorCode::NotFound);
}

// A directory is the one AccessDenied every machine can produce without changing permissions.
CDL_TEST_CASE(FileSystem, DirectoryIsAccessDenied, Unit)
{
	TempDirectory dir;

	const auto read = Platform::ReadFile(dir.Path());
	CDL_CHECK_FALSE(read.has_value());
	CDL_EXPECT_EQ(read.error().Code, FileErrorCode::AccessDenied);

	const auto written = Platform::WriteFile(dir.Path(), Pattern(8, 5));
	CDL_CHECK_FALSE(written.has_value());
	CDL_EXPECT_EQ(written.error().Code, FileErrorCode::AccessDenied);
}

// WriteFile never makes parent directories, so this is what a caller runs first when the tree may not be there yet.
CDL_TEST_CASE(FileSystem, CreateDirectoriesMakesEveryMissingLevel, Unit)
{
	TempDirectory dir;
	const std::filesystem::path deepest = dir / "one" / "two" / "three";

	CDL_CHECK(Platform::CreateDirectories(deepest).has_value());
	CDL_EXPECT(std::filesystem::is_directory(dir / "one"));
	CDL_EXPECT(std::filesystem::is_directory(dir / "one" / "two"));
	CDL_EXPECT(std::filesystem::is_directory(deepest));
	CDL_EXPECT(Platform::WriteFile(deepest / "file.bin", Pattern(8, 7)).has_value());
}

// Callers run it before every save, so a tree that's already there, whole or in part, must succeed and keep what it holds.
CDL_TEST_CASE(FileSystem, CreateDirectoriesSucceedsOverATreeThatIsAlreadyThere, Unit)
{
	TempDirectory dir;
	CDL_CHECK(Platform::CreateDirectories(dir / "kept").has_value());
	CDL_CHECK(Platform::WriteFile(dir / "kept" / "file.bin", Pattern(64, 8)).has_value());

	CDL_EXPECT(Platform::CreateDirectories(dir.Path()).has_value());
	CDL_EXPECT(Platform::CreateDirectories(dir / "kept").has_value());
	CDL_EXPECT(Platform::CreateDirectories(dir / "kept" / "new").has_value());
	CDL_EXPECT(std::filesystem::is_directory(dir / "kept" / "new"));

	const auto read = Platform::ReadFile(dir / "kept" / "file.bin");
	CDL_CHECK(read.has_value());
	CDL_EXPECT(*read == Pattern(64, 8));
}

// A bare file name's parent is empty, meaning the working directory, which is always there.
CDL_TEST_CASE(FileSystem, CreateDirectoriesOfAnEmptyPathSucceeds, Unit)
{
	CDL_EXPECT(Platform::CreateDirectories(std::filesystem::path("file.bin").parent_path()).has_value());
}

// A file can't become a directory, and must neither pass for one nor be replaced by one.
CDL_TEST_CASE(FileSystem, CreateDirectoriesFailsWhereAFileIsInTheWay, Unit)
{
	TempDirectory dir;
	const std::filesystem::path file = dir / "file.bin";
	CDL_CHECK(Platform::WriteFile(file, Pattern(32, 9)).has_value());

	CDL_EXPECT_FALSE(Platform::CreateDirectories(file).has_value());
	CDL_EXPECT_FALSE(Platform::CreateDirectories(file / "below").has_value());

	const auto read = Platform::ReadFile(file);
	CDL_CHECK(read.has_value());
	CDL_EXPECT(*read == Pattern(32, 9));
}

// As for files, this fails if a narrow-string API sneaks into the path handling.
CDL_TEST_CASE(FileSystem, CreateDirectoriesHandlesPathsOutsideTheAnsiCodePage, Unit)
{
	TempDirectory dir;
	const std::filesystem::path deepest = dir / L"папка" / L"フォルダ";

	CDL_CHECK(Platform::CreateDirectories(deepest).has_value());
	CDL_EXPECT(std::filesystem::is_directory(deepest));
}

CDL_TEST_CASE(FileSystem, ToStringNamesEveryCode, Unit)
{
	CDL_EXPECT_EQ(std::string_view(ToString(FileErrorCode::NotFound)), "NotFound"sv);
	CDL_EXPECT_EQ(std::string_view(ToString(FileErrorCode::AccessDenied)), "AccessDenied"sv);
	CDL_EXPECT_EQ(std::string_view(ToString(FileErrorCode::SharingViolation)), "SharingViolation"sv);
	CDL_EXPECT_EQ(std::string_view(ToString(FileErrorCode::TooLarge)), "TooLarge"sv);
	CDL_EXPECT_EQ(std::string_view(ToString(FileErrorCode::IO)), "IO"sv);
}

// FILE_SHARE_READ is what lets these overlap; a reader that excluded others would see SharingViolation here.
CDL_TEST_CASE(FileSystem, ConcurrentReadersAllSeeTheWholeFile, Stress)
{
	TempDirectory dir;
	const std::filesystem::path path = dir / "shared.bin";
	const std::vector<std::byte> expected = Pattern(1 << 20, 6);
	CDL_CHECK(Platform::WriteFile(path, expected).has_value());

	constexpr int kReadsPerThread = 25;
	const int threadCount = StressThreadCount();
	std::vector<int> failedReads(threadCount, 0);
	std::vector<int> wrongContents(threadCount, 0);

	RunConcurrently(threadCount, [&](int thread) {
		for (int i = 0; i < kReadsPerThread; ++i)
		{
			const auto read = Platform::ReadFile(path);
			if (!read)
				++failedReads[thread];
			else if (*read != expected)
				++wrongContents[thread];
		}
	});

	for (int thread = 0; thread < threadCount; ++thread)
	{
		CDL_EXPECT_EQ(failedReads[thread], 0);
		CDL_EXPECT_EQ(wrongContents[thread], 0);
	}
}

// No shared state: threads round-tripping their own files must never see each other's bytes.
CDL_TEST_CASE(FileSystem, ConcurrentWritersToSeparateFilesDoNotInterfere, Stress)
{
	TempDirectory dir;
	constexpr int kRoundTripsPerThread = 50;
	const int threadCount = StressThreadCount();
	std::vector<int> failures(threadCount, 0);

	RunConcurrently(threadCount, [&](int thread) {
		const std::filesystem::path path = dir / std::format("thread-{}.bin", thread);
		for (int i = 0; i < kRoundTripsPerThread; ++i)
		{
			const std::vector<std::byte> content = Pattern(4096 + thread, static_cast<uint32_t>(thread * 1000 + i));
			const auto read = Platform::WriteFile(path, content).and_then([&] { return Platform::ReadFile(path); });
			if (!read || *read != content)
				++failures[thread];
		}
	});

	for (int thread = 0; thread < threadCount; ++thread)
		CDL_EXPECT_EQ(failures[thread], 0);
}

// Two callers making one missing tree race: one finds a level missing, the other makes it, and the first's create then fails
// because it's there. Each round starts every thread together on a fresh tree, so the race keeps coming up.
CDL_TEST_CASE(FileSystem, ConcurrentCreatorsOfOneTreeAllSucceed, Stress)
{
	TempDirectory dir;
	constexpr int kRounds = 25;
	const int threadCount = StressThreadCount();
	std::vector<int> failures(threadCount, 0);

	for (int round = 0; round < kRounds; ++round)
	{
		const std::filesystem::path deepest = dir / std::format("round-{}", round) / "one" / "two";
		RunConcurrently(threadCount, [&](int thread) {
			if (!Platform::CreateDirectories(deepest))
				++failures[thread];
		});
		CDL_EXPECT(std::filesystem::is_directory(deepest));
	}

	for (int thread = 0; thread < threadCount; ++thread)
		CDL_EXPECT_EQ(failures[thread], 0);
}

// What relative asset directories resolve against, so it must be absolute and hold the running binary.
CDL_TEST_CASE(FileSystem, ExecutableDirectoryHoldsTheRunningExecutable, Unit)
{
	const std::filesystem::path dir = Platform::GetExecutableDirectory();
	CDL_EXPECT(dir.is_absolute());
	CDL_EXPECT(std::filesystem::exists(dir / "Candle-Test.exe"));
}
