#include <Candle.h>
#include <Candle/Core/EntryPoint.h>

namespace Candle {

	class SandboxApp : public Application
	{
	public:
		SandboxApp(const ApplicationSpecification& spec)
			: Application(spec)
		{
		}
		virtual ~SandboxApp()
		{
		}

		void OnInit() override
		{
			CDL_INFO(LogChannel::Application, "SandboxApp initialized!");
		}

		void OnShutdown() override
		{
			CDL_INFO(LogChannel::Application, "SandboxApp shudown!");
		}
	};

	Scoped<Application> CreateApplication(int argc, char** argv)
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

		// Logger settings for the Sandbox application
		spec.LoggerSpec.Level = LogLevel::Trace;
		spec.LoggerSpec.ChannelMask = 0xFFFF; // Enable all channels
		spec.LoggerSpec.LogToConsole = true;
		spec.LoggerSpec.LogToFile = true;
		spec.LoggerSpec.LogFilePath = "logs/Sandbox.log";

		return Scoped<SandboxApp>(new SandboxApp(spec));
	}
}