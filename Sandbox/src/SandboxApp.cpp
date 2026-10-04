#include <Candle.h>
#include <Candle/Core/EntryPoint.h>
#include <Candle/Core/Job/JobSystem.h>

#include <thread>

namespace Candle {

	static void TestFunction(uintptr_t jobId, uint32_t)
	{
		CDL_INFO(LogChannel::Application, "Executing job with ID: {}", jobId);
		Platform::SleepCurrentThread(100); // Simulate work
	}

	static void TestBatchFunction(uintptr_t jobId, uint32_t index)
	{
		CDL_INFO(LogChannel::Application, "Executing batch job with ID: {}, index: {}", jobId, index);
		Platform::SleepCurrentThread(100); // Simulate work
	}

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

			m_JobSystem.Start();

			std::array<JobHandle, 1000> jobHandles;
			for (int i = 0; i < 1000; ++i) {
				JobSpec spec = {
					.m_EntryFunc = &TestFunction,
					.m_FuncData = static_cast<uintptr_t>(i),
					.m_Priority = JobPriority::Normal,
					.m_Name = "TestJob"
				};
				jobHandles[i] = m_JobSystem.KickJob(spec);
			}

			// Create group of jobs that depend on the first 10 jobs
			auto handle2 = m_JobSystem.KickJobs(50, {
				.m_EntryFunc = &TestFunction,
				.m_FuncData = 100,
				.m_Priority = JobPriority::High,
				.m_Name = "DependentJob1"
				}, jobHandles);
			auto handle3 = m_JobSystem.KickJobs(25, {
				.m_EntryFunc = &TestBatchFunction,
				.m_FuncData = 100,
				.m_Priority = JobPriority::High,
				.m_Name = "DependentJob2"
				}, jobHandles);
			m_JobSystem.KickJobs(25, {
				.m_EntryFunc = &TestBatchFunction,
				.m_FuncData = 100,
				.m_Priority = JobPriority::High,
				.m_Name = "DependentJobFINAL"
				}, { handle2, handle3 });

			m_Thread = std::jthread([this]() {
				Platform::SleepCurrentThread(60'000);
				m_JobSystem.Stop();
				RequestQuit();
				});
		}

		void OnShutdown() override
		{
			CDL_INFO(LogChannel::Application, "SandboxApp shudown!");
		}

	private:
		std::jthread m_Thread;
		JobSystem m_JobSystem;
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

		// Logger settings for the Sandbox application
		spec.LoggerSpec.Level = LogLevel::Trace;
		spec.LoggerSpec.ChannelMask = 0xFFFF; // Enable all channels
		spec.LoggerSpec.LogToConsole = true;
		spec.LoggerSpec.LogToFile = true;
		spec.LoggerSpec.LogFilePath = "logs/Sandbox.log";

		return ScopedPtr<SandboxApp>(new SandboxApp(spec));
	}
}