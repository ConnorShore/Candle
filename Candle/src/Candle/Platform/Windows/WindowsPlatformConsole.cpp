#include "cdlpch.h"
#include "Candle/Platform/Platform.h"

#ifndef CDL_PLATFORM_WINDOWS
#error "WindowsPlatformThread.cpp is Windows-only."
#endif

#include <Windows.h>

namespace Candle {

	bool Platform::EnableConsoleAnsiColors()
	{
		const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
		if (out == INVALID_HANDLE_VALUE)
			return false;
		DWORD mode = 0;
		if (!GetConsoleMode(out, &mode))
			return false;
		return SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
	}

	bool Platform::DisableConsoleAnsiColors()
	{
		const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
		if (out == INVALID_HANDLE_VALUE)
			return false;
		DWORD mode = 0;
		if (!GetConsoleMode(out, &mode))
			return false;
		return SetConsoleMode(out, mode & ~ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
	}

}