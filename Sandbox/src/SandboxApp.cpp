#include <Candle.h>
#include <Candle/App/EntryPoint.h>
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
			CDL_INFO(LogChannel::Application, "SandboxApp shutdown!");
		}
	};

	ScopedPtr<Application> CreateApplication(int argc, char** argv)
	{
		ApplicationSpecification spec;
		spec.AppInfo.Name = "Candle Sandbox";
		spec.AppInfo.Version = { 1, 0, 0 };

		// A path typed in a shell means one relative to that shell, so anchor overrides to the working directory here
		if (argc >= 3)
			spec.EngineAssetDir = std::filesystem::absolute(argv[2]);
		if (argc >= 4)
			spec.ProjectAssetDir = std::filesystem::absolute(argv[3]);

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

		// Render settings for the Sandbox application
		spec.RenderSpec.DevicePreference = RenderDevicePreference::DiscreteGPU;

		return ScopedPtr<SandboxApp>::Create(spec);
	}
}