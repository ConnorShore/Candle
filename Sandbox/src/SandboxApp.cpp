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
			std::cout << "SandboxApp initialized!" << std::endl;
		}

		void OnShutdown() override
		{
			std::cout << "SandboxApp shutdown!" << std::endl;
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

		return Scoped<SandboxApp>(new SandboxApp(spec));
	}
}