#include <Candle.h>
#include <Candle/Core/EntryPoint.h>
#include <Candle/Core/Job/JobSystem.h>

#include <cmath>

namespace Candle {

	class SandboxApp : public Application
	{
	public:
		SandboxApp(const ApplicationSpecification& spec)
			: Application(spec)
		{
		}
		virtual ~SandboxApp() = default;

		void OnInit() override
		{
			CDL_INFO(LogChannel::Application, "SandboxApp initialized!");
		}

		void OnShutdown() override
		{
			CDL_INFO(LogChannel::Application, "SandboxApp shudown!");
		}
	};

	ScopedPtr<Application> CreateApplication(int argc, char** argv)
	{
		ApplicationSpecification spec;
		spec.Name = "Candle Sandbox";

		std::string engineAssetDir = "CandleCore";
		if (argc >= 3) {
			engineAssetDir = argv[2];
		}

		std::string projectAssetDir = "GameData";
		if (argc >= 4) {
			projectAssetDir = argv[3];
		}

		// The Editor explicitly points to the source code folders
		spec.EngineAssetDir = engineAssetDir;
		spec.ProjectAssetDir = projectAssetDir;

		spec.CommandLineArgsCount = argc;
		spec.CommandLineArgs = argv;

		// Window settings for the Sandbox application
		spec.WindowSpec.Title = "Candle Sandbox";
		spec.WindowSpec.Width = 1600;
		spec.WindowSpec.Height = 900;
		spec.WindowSpec.Mode = WindowMode::Windowed;

		// Logger settings for the Sandbox application
		spec.LoggerSpec.Level = LogLevel::Trace;
		spec.LoggerSpec.ChannelMask = 0xFFFF; // Enable all channels
		spec.LoggerSpec.LogToConsole = true;
		spec.LoggerSpec.LogToFile = true;
		spec.LoggerSpec.LogFilePath = "logs/Sandbox.log";

		return ScopedPtr<SandboxApp>::Create(spec);
	}
}