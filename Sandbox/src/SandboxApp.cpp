#include <Candle.h>
#include <Candle/Core/EntryPoint.h>
#include <Candle/Core/Job/JobSystem.h>

#include <cmath>

namespace Candle {

	// Profiler sample: a fake game frame, kicked from a "Game" thread, shaped like a real engine's job graph.
	//   Animate --> Simulate --> Cull --+
	//   Think (AI, Low priority) -------+--> BuildRenderPacket (one job: the frame's serial tail)
	// Every k_SpikeInterval frames a few entities in Simulate's first chunk get 100x the work, to show a hitch
	// and the load imbalance one slow chunk causes.
	// Threading: index i of a stage writes only element i; stages are ordered by job dependencies, and the
	// game thread touches the arrays only between WaitForJob and the next frame's kicks.
	struct SampleWorld
	{
		static constexpr uint32_t k_EntityCount = 16'384;
		static constexpr uint32_t k_AgentCount = 2'048;
		static constexpr uint64_t k_SpikeInterval = 120;

		std::vector<float> m_Pose = std::vector<float>(k_EntityCount);
		std::vector<float> m_Position = std::vector<float>(k_EntityCount);
		std::vector<float> m_Thought = std::vector<float>(k_AgentCount);
		std::vector<uint8_t> m_Visible = std::vector<uint8_t>(k_EntityCount);	// Not vector<bool>: packed bits would race between chunks
		uint64_t m_FrameIndex = 0;
		float m_Checksum = 0.0f;
	};

	// Deterministic busy work standing in for real per-entity cost.
	static float Burn(float seed, uint32_t iterations)
	{
		float x = seed;
		for (uint32_t i = 0; i < iterations; ++i)
			x = std::sin(x) * 0.5f + std::cos(x * 1.3f);
		return x;
	}

	static SampleWorld& World(uintptr_t data) { return *reinterpret_cast<SampleWorld*>(data); }

	static void Animate(uintptr_t data, uint32_t i)
	{
		SampleWorld& world = World(data);
		world.m_Pose[i] = Burn(static_cast<float>(i + world.m_FrameIndex), 40);
	}

	static void Simulate(uintptr_t data, uint32_t i)
	{
		SampleWorld& world = World(data);
		const bool spike = world.m_FrameIndex % SampleWorld::k_SpikeInterval == 0 && i < 32;
		world.m_Position[i] += Burn(world.m_Pose[i], spike ? 25'000 : 40) * 0.016f;
	}

	static void Cull(uintptr_t data, uint32_t i)
	{
		SampleWorld& world = World(data);
		world.m_Visible[i] = Burn(world.m_Position[i], 15) > 0.0f;
	}

	static void Think(uintptr_t data, uint32_t i)
	{
		SampleWorld& world = World(data);
		world.m_Thought[i] = Burn(world.m_Thought[i] + static_cast<float>(i), 250);
	}

	static void BuildRenderPacket(uintptr_t data, uint32_t)
	{
		SampleWorld& world = World(data);
		float packet = 0.0f;
		for (uint32_t i = 0; i < SampleWorld::k_EntityCount; ++i)
			if (world.m_Visible[i])
				packet += Burn(world.m_Position[i] + world.m_Thought[i % SampleWorld::k_AgentCount], 8);
		world.m_Checksum = packet;
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
			m_GameThread.Start([this](std::stop_token stoken) { RunSample(stoken); });
		}

		void OnShutdown() override
		{
			m_GameThread.Join();
			CDL_INFO(LogChannel::Application, "SandboxApp shudown! Ran {} sample frames", m_World.m_FrameIndex);
		}

	private:
		// Game thread. Frames run back to back, unpaced, so each frame's length in Tracy is its work, not a sleep.
		void RunSample(std::stop_token stoken)
		{
			const Tick start = Platform::GetTick();
			while (!stoken.stop_requested() && Platform::ToSeconds(start, Platform::GetTick()) < k_RunSeconds)
				RunFrame();

			// Only after the last WaitForJob has returned, so no frame can block on a stopped pool.
			m_JobSystem.Stop();
			RequestQuit();
		}

		void RunFrame()
		{
			// Stands in for the serial part of a frame: input, game logic, deciding what to kick.
			{
				CDL_PROFILE_SCOPE("GameUpdate");
				m_World.m_FrameIndex++;
				m_World.m_Checksum = Burn(m_World.m_Checksum, 20'000);
			}

			const uintptr_t world = reinterpret_cast<uintptr_t>(&m_World);
			const uint32_t entities = SampleWorld::k_EntityCount;

			const JobHandle animate = m_JobSystem.KickJobs(entities, { .m_EntryFunc = &Animate, .m_FuncData = world, .m_Name = "Animate" });
			const JobHandle simulate = m_JobSystem.KickJobs(entities, { .m_EntryFunc = &Simulate, .m_FuncData = world, .m_Name = "Simulate" }, { animate });
			const JobHandle cull = m_JobSystem.KickJobs(entities, { .m_EntryFunc = &Cull, .m_FuncData = world, .m_Name = "Cull" }, { simulate });
			const JobHandle think = m_JobSystem.KickJobs(SampleWorld::k_AgentCount, {
				.m_EntryFunc = &Think, .m_FuncData = world, .m_Priority = JobPriority::Low, .m_Name = "Think" });
			const JobHandle packet = m_JobSystem.KickJob({
				.m_EntryFunc = &BuildRenderPacket, .m_FuncData = world, .m_Priority = JobPriority::High, .m_Name = "BuildRenderPacket" },
				{ cull, think });

			m_JobSystem.WaitForJob(packet);
			CDL_PROFILE_FRAME();
		}

	private:
		static constexpr double k_RunSeconds = 60.0;

		JobSystem m_JobSystem;
		SampleWorld m_World;
		Thread m_GameThread{ "Game" };	// Declared last so it is destroyed (joined) before the job system and the world
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