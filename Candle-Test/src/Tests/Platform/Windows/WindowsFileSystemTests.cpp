// The Windows backend's share modes and their mapping to FileErrorCode::SharingViolation. Holding a file open
// takes a raw HANDLE, so, as Platform/SDL is for SDL, this directory is the only one that includes Windows.h.

#include "TestFramework.h"
#include "TestHelpers.h"

#ifndef NOMINMAX
	#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <filesystem>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Unit;

namespace {

	// Another process's handle on the file, held for the test's duration.
	class HeldOpen
	{
	public:
		HeldOpen(const std::filesystem::path& path, DWORD access, DWORD shareMode)
			: m_Handle(::CreateFileW(path.c_str(), access, shareMode, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr))
		{
			CDL_CHECK_MSG(m_Handle != INVALID_HANDLE_VALUE, std::format("CreateFileW failed: {}", ::GetLastError()));
		}
		~HeldOpen() { if (m_Handle != INVALID_HANDLE_VALUE) ::CloseHandle(m_Handle); }

		HeldOpen(const HeldOpen&) = delete;
		HeldOpen& operator=(const HeldOpen&) = delete;

	private:
		HANDLE m_Handle;
	};

	std::filesystem::path WriteFixture(const TempDirectory& dir)
	{
		const std::filesystem::path path = dir / "held.bin";
		const std::byte content[] = { std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 }, std::byte{ 4 } };
		CDL_CHECK(Platform::WriteFile(path, content).has_value());
		return path;
	}

	void ExpectSharingViolation(const FileError& error)
	{
		CDL_EXPECT_EQ(error.Code, FileErrorCode::SharingViolation);
		CDL_EXPECT_EQ(error.OSError, static_cast<uint32_t>(ERROR_SHARING_VIOLATION));
	}

}

CDL_TEST_CASE(WindowsFileSystem, AnExclusiveHandleBlocksReadsAndWrites, Unit)
{
	TempDirectory dir;
	const std::filesystem::path path = WriteFixture(dir);
	HeldOpen exclusive(path, GENERIC_READ | GENERIC_WRITE, 0);

	const auto read = Platform::ReadFile(path);
	CDL_CHECK_FALSE(read.has_value());
	ExpectSharingViolation(read.error());

	const auto text = Platform::ReadTextFile(path);
	CDL_CHECK_FALSE(text.has_value());
	ExpectSharingViolation(text.error());

	const auto written = Platform::WriteFile(path, {});
	CDL_CHECK_FALSE(written.has_value());
	ExpectSharingViolation(written.error());
}

// The hot-reload case: a compiler mid-write shares freely, but the read shares only with readers, so it is refused.
CDL_TEST_CASE(WindowsFileSystem, AWriterMakesAReadASharingViolation, Unit)
{
	TempDirectory dir;
	const std::filesystem::path path = WriteFixture(dir);
	HeldOpen writer(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE);

	const auto read = Platform::ReadFile(path);
	CDL_CHECK_FALSE(read.has_value());
	ExpectSharingViolation(read.error());
}

// The read's FILE_SHARE_READ admits other readers; the write's exclusive open does not.
CDL_TEST_CASE(WindowsFileSystem, AReaderAdmitsReadsButBlocksWrites, Unit)
{
	TempDirectory dir;
	const std::filesystem::path path = WriteFixture(dir);
	HeldOpen reader(path, GENERIC_READ, FILE_SHARE_READ);

	const auto read = Platform::ReadFile(path);
	CDL_CHECK(read.has_value());
	CDL_EXPECT_EQ(read->size(), 4u);

	const auto written = Platform::WriteFile(path, {});
	CDL_CHECK_FALSE(written.has_value());
	ExpectSharingViolation(written.error());
}
