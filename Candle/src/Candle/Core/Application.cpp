#include "cdlpch.h"
#include "Application.h"

#include "Candle/Render/RenderManager.h"
#include "Candle/Render/FramePacket.h"

#include "Candle/Platform/Platform.h"

// Temporary, until we have a better way to register asset handlers //
#include "Candle/Render/Assets/MeshAsset.h"
//

namespace Candle {

	Application::PlatformScope::PlatformScope() { Platform::Init(); }
	Application::PlatformScope::~PlatformScope() { Platform::Shutdown(); }

	Application::LoggerScope::LoggerScope(const LoggerSpecification& spec) { Logger::Init(spec); }
	Application::LoggerScope::~LoggerScope() { Logger::Shutdown(); }

	Application::WindowingScope::WindowingScope(bool enabled) : m_Enabled(enabled) { if (m_Enabled) Platform::InitWindowing(); }
	Application::WindowingScope::~WindowingScope() { if (m_Enabled) Platform::ShutdownWindowing(); }

	Application::Application(const ApplicationSpecification& appSpecs)
		: m_Specification(appSpecs)
		, m_LoggerScope(m_Specification.LoggerSpec)
		, m_WindowingScope(!m_Specification.Headless)
		, m_Window(m_Specification.Headless ? ScopedPtr<Window>() : ScopedPtr<Window>::Create(m_Specification.WindowSpec))
		, m_RenderManager(m_Specification.Headless ? ScopedPtr<RenderManager>() : ScopedPtr<RenderManager>::Create(m_Specification.RenderSpec, m_Specification.AppInfo, *m_Window, m_JobSystem))
	{
		CDL_CORE_INFO(LogChannel::Application, "Application created: {}", m_Specification.AppInfo.Name);
		Platform::SetCurrentThreadName("Candle-MainThread");

		// Register asset handlers for various subsystems
		if (m_RenderManager)
			m_RenderManager->RegisterAssetHandlers(m_AssetManager);

		// TEMPORARY FOR TESTING
		MeshData testMeshData{
			.Vertices = {
				{ { 0.5f, -0.5f, 0.0f } },
				{ { 0.0f, 0.5f, 0.0f } },
				{ { -0.5f, -0.5f, 0.0f } }
			},
			.Indices = { 0, 1, 2 }
		};
		SharedPtr<MeshAsset> meshAsset = SharedPtr<MeshAsset>::Create(std::move(testMeshData));
		m_AssetManager.LoadAsset("TestMesh", meshAsset);
	}

	Application::~Application()
	{
		CDL_CORE_INFO(LogChannel::Application, "Application destroyed: {}", m_Specification.AppInfo.Name);
	}

	void Application::Run()
	{
		SharedPtr<MeshAsset> meshAsset = m_AssetManager.GetAsset<MeshAsset>("TestMesh");
		std::vector<GPUMeshHandle> meshes{ meshAsset->GetGPUHandle() };

		// Spawn the render thread if we have a render manager
		if (m_RenderManager)
			m_RenderManager->SpawnRenderThread();

		Tick startTick = Platform::GetTick();

		while (!IsQuitRequested())
		{
			CDL_PROFILE_SCOPE("Application::Run");

			// Process events
			HandlePlatformEvents();

			// Moves inside the fixed-step tick loop once there is one, so each tick consumes the edges exactly once
			InputSnapshot inputSnapshot = m_InputSystem.CaptureSnapshot();

			// TODO: Implement game logic
			if (m_RenderManager)
			{
				CDL_PROFILE_SCOPE("Application::Run::FramePacket");
				FramePacket& packet = m_RenderManager->BeginFramePacket();
				CDL_PROFILE_SCOPE_METADATA(packet.FrameIndex);
				packet.Meshes = meshes;
				packet.Time = m_FrameStats.TotalTime;
				packet.Minimized = m_Window->IsMinimized();
				packet.SurfaceSize = m_Window->GetSize();
				m_RenderManager->SubmitFramePacket(packet);
			}

			// Update the frame stats
			Tick endTick = Platform::GetTick();
			m_FrameStats.DeltaTime = TimeStep(static_cast<float>(Platform::ToSeconds(startTick, endTick)));
			m_FrameStats.TotalTime += m_FrameStats.DeltaTime;
			m_FrameStats.FrameCount++;
			startTick = endTick;

			// Display log every 1/2 second, averaged over the window: one frame's 1/dt depends on where it lands in the present cycle
			m_FrameStats.TimeSinceLastLog += m_FrameStats.DeltaTime;
			m_FrameStats.FramesSinceLastLog++;
			m_FrameStats.WorstFrameTime = std::max(m_FrameStats.WorstFrameTime, m_FrameStats.DeltaTime.GetMilliseconds());
			if (m_FrameStats.TimeSinceLastLog >= 0.5f)
			{
				CDL_CORE_INFO(LogChannel::Application, "Application running: {}; Avg FPS: {:.0f} ({:.2f} ms), worst frame: {:.2f} ms",
					m_Specification.AppInfo.Name,
					m_FrameStats.FramesSinceLastLog / m_FrameStats.TimeSinceLastLog,
					m_FrameStats.TimeSinceLastLog * 1000.0 / m_FrameStats.FramesSinceLastLog,
					m_FrameStats.WorstFrameTime);

				m_FrameStats.TimeSinceLastLog = 0.0f;
				m_FrameStats.FramesSinceLastLog = 0;
				m_FrameStats.WorstFrameTime = 0.0f;
			}

			CDL_PROFILE_FRAME_NAMED("SimFrame");
		}

		// Wait for the render thread to finish before exiting
		if (m_RenderManager)
			m_RenderManager->JoinRenderThread();
	}

	void Application::HandlePlatformEvents()
	{
		// If headless, we don't have a window to receive events from, so we skip event handling
		if (m_Specification.Headless)
			return;

		Platform::PumpEvents(m_FrameEvents);
		for (auto& evt : m_FrameEvents)
		{
			std::visit(Overloaded{
				[&](const QuitRequested&) { RequestQuit(); },
				[&](const WindowCloseRequested& e) { if (e.WindowID == m_Window->GetID()) RequestQuit(); },
				[&](const WindowResized& e) {},
				[&](const WindowFocus& e) {},
				[&](const WindowMinimized& e) {},
				[&](const InputEvent& e) { m_InputSystem.ConsumeEvent(e); }
				}, evt);
		}
	}

}
