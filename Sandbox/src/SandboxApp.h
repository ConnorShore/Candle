#pragma once

#include <Candle.h>
#include <Candle/App/EntryPoint.h>

namespace Candle {

	class SandboxApp : public Application
	{
	public:
		SandboxApp(const ApplicationSpecification& spec);
		virtual ~SandboxApp() = default;

		void OnInit(ApplicationContext context) override;
		void OnUpdate(UpdateContext context) override;

		void OnShutdown() override;

	private:
		CameraLens m_CameraLens{ PerspectiveLens{ .VerticalFov = glm::radians(60.0f), .Near = 0.1f, .Far = 100.0f } };
		CameraPose m_CameraPose{ .Position = glm::vec3(2.0f, 2.0f, 2.0f) };	// OnInit aims it at the cubes
		SimpleController m_CameraController{ .MoveSpeed = 2.0f, .LookSpeed = 0.5f };
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
		spec.RenderSpec.RenderToSwapChain = true;	// false renders the world into the "Scene" window instead

		// Enable shader printf when debugging shaders
		//spec.RenderSpec.ValidationSpec.EnableShaderPrintf = true;

		return ScopedPtr<SandboxApp>::Create(spec);
	}
}