#include "cdlpch.h"
#include "Application.h"

#include "Candle/Render/RenderManager.h"
#include "Candle/Render/FramePacket.h"

#include "Candle/Platform/Platform.h"

// Temporary, until we have a better way to register asset handlers //
#include "Candle/Render/Assets/MeshAsset.h"
#include "Candle/Render/Assets/Texture.h"
#include <glm/gtc/matrix_transform.hpp>
//


namespace Candle {

	namespace {

		// Resolved once, so nothing downstream depends on the working directory
		ApplicationSpecification ResolveAssetDirectories(ApplicationSpecification spec)
		{
			const std::filesystem::path exeDir = Platform::GetExecutableDirectory();
			if (spec.EngineAssetDir.is_relative())
				spec.EngineAssetDir = exeDir / spec.EngineAssetDir;
			if (spec.ProjectAssetDir.is_relative())
				spec.ProjectAssetDir = exeDir / spec.ProjectAssetDir;
			return spec;
		}

	}

	Application::PlatformScope::PlatformScope() { Platform::Init(); }
	Application::PlatformScope::~PlatformScope() { Platform::Shutdown(); }

	Application::LoggerScope::LoggerScope(const LoggerSpecification& spec) { Logger::Init(spec); }
	Application::LoggerScope::~LoggerScope() { Logger::Shutdown(); }

	Application::WindowingScope::WindowingScope(bool enabled) : Enabled(enabled)
	{
		if (!Enabled)
			return;

		auto result = Platform::InitWindowing();
		if (!result)
			CDL_CORE_PANIC(LogChannel::Window, "Failed to initialize windowing: {}", result.error());
	}
	Application::WindowingScope::~WindowingScope() { if (Enabled) Platform::ShutdownWindowing(); }

	Application::Application(const ApplicationSpecification& appSpecs)
		: m_Specification(ResolveAssetDirectories(appSpecs))
		, m_LoggerScope(m_Specification.LoggerSpec)
		, m_WindowingScope(!m_Specification.Headless)
		, m_Window(m_Specification.Headless ? ScopedPtr<Window>() : ScopedPtr<Window>::Create(m_Specification.WindowSpec))
		, m_RenderManager(m_Specification.Headless ? ScopedPtr<RenderManager>() : ScopedPtr<RenderManager>::Create(m_Specification.RenderSpec, m_Specification.AppInfo, m_Specification.EngineAssetDir, *m_Window, m_JobSystem))
	{
		CDL_CORE_INFO(LogChannel::Application, "Application created: {}", m_Specification.AppInfo.Name);
		Platform::SetCurrentThreadName("Candle-MainThread");

		// Register asset handlers for various subsystems
		if (m_RenderManager)
			m_RenderManager->RegisterAssetHandlers(m_AssetManager);
	}

	Application::~Application()
	{
		CDL_CORE_INFO(LogChannel::Application, "Application destroyed: {}", m_Specification.AppInfo.Name);
	}

	void Application::Run()
	{
		// Call the application init callback
		OnInit({ m_AssetManager, m_ActiveWorld });

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

			// Call the application update callback
			ApplicationContext appContext{ m_AssetManager, m_ActiveWorld };
			UpdateContext updateContext{ appContext, m_FrameStats.DeltaTime, inputSnapshot };
			OnUpdate(updateContext);

			// TODO: Implement game logic
			if (m_RenderManager)
			{
				CDL_PROFILE_SCOPE("Application::Run::FramePacket");
				FramePacket& packet = m_RenderManager->BeginFramePacket();
				CDL_PROFILE_SCOPE_METADATA(packet.FrameIndex);
				ExtractToFramePacket(packet);
				m_RenderManager->SubmitFramePacket(packet);

				// Nothing is presented while minimized, so nothing else holds the loop back
				if (m_Window->IsMinimized())
					Platform::SleepCurrentThread(10);
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

		// Call the application shutdown callback
		OnShutdown();
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

	void Application::ExtractToFramePacket(FramePacket& packet)
	{
		std::vector<DrawItem> drawItems;
		std::vector<glm::mat4> transforms;
		drawItems.reserve(m_ActiveWorld.Entities.size());
		transforms.reserve(m_ActiveWorld.Entities.size());

		for (size_t i = 0; i < m_ActiveWorld.Entities.size(); ++i)
		{
			Entity& entity = m_ActiveWorld.Entities[i];
			auto mesh = m_AssetManager.GetAsset<MeshAsset>(entity.MeshID);
			auto texture = m_AssetManager.GetAsset<Texture>(entity.TextureID);

			transforms.push_back(entity.Transform);
			drawItems.push_back({
				.Mesh = mesh->GetGPUHandle(),
				.Texture = texture ? texture->GetGPUHandle() : GPUTextureHandle{},
				.Sampler = SamplerType::LinearClamp,
				.TransformIndex = static_cast<uint32_t>(drawItems.size())
			});
		}

		packet.DrawItems = drawItems;
		packet.TransformData = transforms;
		packet.ViewMatrix = m_ActiveWorld.View;
		packet.ProjectionMatrix = m_ActiveWorld.Projection;
		packet.Time = m_FrameStats.TotalTime;
		packet.Minimized = m_Window->IsMinimized();
		packet.SurfaceSize = m_Window->GetSize();
	}

}
