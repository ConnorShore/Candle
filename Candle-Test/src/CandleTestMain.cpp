#include <Candle.h>

#include "TestFramework.h"

#include <cstdlib>
#include <string>
#include <string_view>

namespace {

	// Command-line flags win over the matching environment variable.
	//   Candle-Test.exe --filter=unit --run=Logger
	std::string ReadOption(int argc, char** argv, std::string_view prefix, const char* envName)
	{
		for (int i = 1; i < argc; ++i)
		{
			const std::string_view arg = argv[i];
			if (arg.starts_with(prefix))
				return std::string(arg.substr(prefix.size()));
		}

		const char* fromEnv = std::getenv(envName);
		return fromEnv ? std::string(fromEnv) : std::string();
	}

	bool HasFlag(int argc, char** argv, std::string_view flag, const char* envName)
	{
		for (int i = 1; i < argc; ++i)
		{
			if (flag == argv[i])
				return true;
		}
		return std::getenv(envName) != nullptr;
	}

}

int main(int argc, char** argv)
{
	// No Application: the Logger tests need to own Logger::Init/Shutdown themselves.
	// Platform time still has to be initialised, or every tick conversion divides by zero.
	Candle::Platform::Init();

	if (HasFlag(argc, argv, "--list", "CDL_TEST_LIST"))
	{
		Candle::Test::ListTests();
		return 0;
	}

	Candle::Test::RunOptions options;
	options.TypeFilter = ReadOption(argc, argv, "--filter=", "CDL_TEST_FILTER");
	options.NameFilter = ReadOption(argc, argv, "--run=", "CDL_TEST_RUN");

	return Candle::Test::RunAll(options) == 0 ? 0 : 1;
}
