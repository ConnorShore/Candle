#include "cdlpch.h"
#include "Candle/Platform/Platform.h"

#ifndef CDL_PLATFORM_WINDOWS
#error "WindowsPlatformFileSystem.cpp is Windows-only."
#endif

#include <Windows.h>

namespace Candle {

	namespace {
		constexpr std::string_view Utf8Bom = "\xEF\xBB\xBF";

		// RAII wrapper for a Windows HANDLE
		struct ScopedWinHandle {
			HANDLE Handle;

			ScopedWinHandle(HANDLE h) : Handle(h) {}
			~ScopedWinHandle() {
				if (Handle != INVALID_HANDLE_VALUE)
					::CloseHandle(Handle);
			}

			// Delete copy constructor and assignment operator to prevent copying
			ScopedWinHandle(const ScopedWinHandle&) = delete;
			ScopedWinHandle& operator=(const ScopedWinHandle&) = delete;
		};

		FileError LastFileError()
		{
			const DWORD osError = ::GetLastError();
			switch (osError)
			{
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:		return { FileErrorCode::NotFound, osError };
			case ERROR_ACCESS_DENIED:		return { FileErrorCode::AccessDenied, osError };
			case ERROR_SHARING_VIOLATION:
			case ERROR_LOCK_VIOLATION:		return { FileErrorCode::SharingViolation, osError };
			default:						return { FileErrorCode::IO, osError };
			}
		}

		// Templated on the container so ReadTextFile reads straight into a std::string, with no copy out of a byte vector.
		template<typename Container>
		std::expected<Container, FileError> ReadWholeFile(const std::filesystem::path& path)
		{
			HANDLE hRawFile = ::CreateFileW(
				path.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ,        // Allow other processes to read concurrently
				nullptr,
				OPEN_EXISTING,          // Fail if the file doesn't exist
				FILE_ATTRIBUTE_NORMAL,
				nullptr
			);

			if (hRawFile == INVALID_HANDLE_VALUE)
				return std::unexpected(LastFileError());

			ScopedWinHandle scopedHandle(hRawFile); // Ensure the handle is closed when going out of scope

			// Get the exact file size safely
			LARGE_INTEGER fileSize;
			if (!::GetFileSizeEx(scopedHandle.Handle, &fileSize))
				return std::unexpected(LastFileError());

			// A runtime error, not an assert: the size comes from the disk, and the cast below would truncate it.
			if (fileSize.QuadPart > MAXDWORD)
				return std::unexpected(FileError{ FileErrorCode::TooLarge, 0 });

			// Pre-allocate buffer matching file size
			Container buffer(static_cast<size_t>(fileSize.QuadPart), typename Container::value_type{});
			DWORD bytesRead = 0;

			// Read file contents into buffer
			BOOL success = ::ReadFile(
				scopedHandle.Handle,
				buffer.data(),
				static_cast<DWORD>(buffer.size()),
				&bytesRead,
				nullptr
			);

			if (!success)
				return std::unexpected(LastFileError());

			if (bytesRead != static_cast<DWORD>(buffer.size()))
				return std::unexpected(FileError{ FileErrorCode::IO, ERROR_HANDLE_EOF });

			return buffer;
		}

	}

	std::expected<std::vector<std::byte>, FileError> Platform::ReadFile(const std::filesystem::path& path)
	{
		return ReadWholeFile<std::vector<std::byte>>(path);
	}

	std::expected<std::string, FileError> Platform::ReadTextFile(const std::filesystem::path& path)
	{
		auto text = ReadWholeFile<std::string>(path);

		// Editors (Visual Studio among them) can save UTF-8 with a byte-order mark; it marks the encoding, it isn't text.
		if (text && text->starts_with(Utf8Bom))
			text->erase(0, Utf8Bom.size());

		return text;
	}

	std::expected<void, FileError> Platform::WriteFile(const std::filesystem::path& path, std::span<const std::byte> data)
	{
		// Before the open, because CREATE_ALWAYS truncates the existing file as it opens it.
		if (data.size() > MAXDWORD)
			return std::unexpected(FileError{ FileErrorCode::TooLarge, 0 });

		// Open/Create the file using Unicode variant CreateFileW
		HANDLE hRawFile = ::CreateFileW(
			path.c_str(),
			GENERIC_WRITE,
			0,                      // No sharing
			nullptr,                // Default security
			CREATE_ALWAYS,          // Overwrite existing / create new
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);

		if (hRawFile == INVALID_HANDLE_VALUE)
			return std::unexpected(LastFileError());

		ScopedWinHandle scopedHandle(hRawFile); // Ensure the handle is closed when going out of scope

		DWORD bytesWritten = 0;

		// Perform the synchronous write operation
		BOOL success = ::WriteFile(
			scopedHandle.Handle,
			data.data(),
			static_cast<DWORD>(data.size()),
			&bytesWritten,
			nullptr
		);

		if (!success)
			return std::unexpected(LastFileError());

		if (bytesWritten != static_cast<DWORD>(data.size()))
			return std::unexpected(FileError{ FileErrorCode::IO, ERROR_WRITE_FAULT });

		return {};
	}

}