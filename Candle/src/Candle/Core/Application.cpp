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
				// +Z
				{ { -0.5f, -0.5f,  0.5f }, 0.0f, {  0.0f,  0.0f,  1.0f }, 0.0f },
				{ {  0.5f, -0.5f,  0.5f }, 1.0f, {  0.0f,  0.0f,  1.0f }, 0.0f },
				{ {  0.5f,  0.5f,  0.5f }, 1.0f, {  0.0f,  0.0f,  1.0f }, 1.0f },
				{ { -0.5f,  0.5f,  0.5f }, 0.0f, {  0.0f,  0.0f,  1.0f }, 1.0f },
				// -Z
				{ {  0.5f, -0.5f, -0.5f }, 0.0f, {  0.0f,  0.0f, -1.0f }, 0.0f },
				{ { -0.5f, -0.5f, -0.5f }, 1.0f, {  0.0f,  0.0f, -1.0f }, 0.0f },
				{ { -0.5f,  0.5f, -0.5f }, 1.0f, {  0.0f,  0.0f, -1.0f }, 1.0f },
				{ {  0.5f,  0.5f, -0.5f }, 0.0f, {  0.0f,  0.0f, -1.0f }, 1.0f },
				// +X
				{ {  0.5f, -0.5f,  0.5f }, 0.0f, {  1.0f,  0.0f,  0.0f }, 0.0f },
				{ {  0.5f, -0.5f, -0.5f }, 1.0f, {  1.0f,  0.0f,  0.0f }, 0.0f },
				{ {  0.5f,  0.5f, -0.5f }, 1.0f, {  1.0f,  0.0f,  0.0f }, 1.0f },
				{ {  0.5f,  0.5f,  0.5f }, 0.0f, {  1.0f,  0.0f,  0.0f }, 1.0f },
				// -X
				{ { -0.5f, -0.5f, -0.5f }, 0.0f, { -1.0f,  0.0f,  0.0f }, 0.0f },
				{ { -0.5f, -0.5f,  0.5f }, 1.0f, { -1.0f,  0.0f,  0.0f }, 0.0f },
				{ { -0.5f,  0.5f,  0.5f }, 1.0f, { -1.0f,  0.0f,  0.0f }, 1.0f },
				{ { -0.5f,  0.5f, -0.5f }, 0.0f, { -1.0f,  0.0f,  0.0f }, 1.0f },
				// +Y
				{ { -0.5f,  0.5f,  0.5f }, 0.0f, {  0.0f,  1.0f,  0.0f }, 0.0f },
				{ {  0.5f,  0.5f,  0.5f }, 1.0f, {  0.0f,  1.0f,  0.0f }, 0.0f },
				{ {  0.5f,  0.5f, -0.5f }, 1.0f, {  0.0f,  1.0f,  0.0f }, 1.0f },
				{ { -0.5f,  0.5f, -0.5f }, 0.0f, {  0.0f,  1.0f,  0.0f }, 1.0f },
				// -Y
				{ { -0.5f, -0.5f, -0.5f }, 0.0f, {  0.0f, -1.0f,  0.0f }, 0.0f },
				{ {  0.5f, -0.5f, -0.5f }, 1.0f, {  0.0f, -1.0f,  0.0f }, 0.0f },
				{ {  0.5f, -0.5f,  0.5f }, 1.0f, {  0.0f, -1.0f,  0.0f }, 1.0f },
				{ { -0.5f, -0.5f,  0.5f }, 0.0f, {  0.0f, -1.0f,  0.0f }, 1.0f }
			},
			.Indices = {
				 0,  1,  2,   2,  3,  0,
				 4,  5,  6,   6,  7,  4,
				 8,  9, 10,  10, 11,  8,
				12, 13, 14,  14, 15, 12,
				16, 17, 18,  18, 19, 16,
				20, 21, 22,  22, 23, 20
			}
		};

		SharedPtr<MeshAsset> meshAsset = SharedPtr<MeshAsset>::Create("TestMesh", std::move(testMeshData));
		m_AssetManager.LoadAsset(meshAsset);

		TextureData data{
			.Sampler{SamplerType::LinearWrap}
		};
		SharedPtr<Texture> textureAsset = SharedPtr<Texture>::Create("TestTexture", "Candle/res/textures/box.jpg", std::move(data));
		uint64_t texUUID = m_AssetManager.LoadAsset(textureAsset);
	}

	Application::~Application()
	{
		CDL_CORE_INFO(LogChannel::Application, "Application destroyed: {}", m_Specification.AppInfo.Name);
	}

	void Application::Run()
	{
		// Sample test data setup (temporary)
		SharedPtr<MeshAsset> meshAsset = m_AssetManager.GetAsset<MeshAsset>("TestMesh");
		SharedPtr<Texture> textureAsset = m_AssetManager.GetAsset<Texture>("TestTexture");

		// Depth check: a textured cube spinning at the origin, and a larger magenta one wholly behind it but overlapping it on
		// screen. The far cube is drawn second, so without depth testing it paints over the near one where they overlap.
		const glm::vec3 eye{ 2.0f, 2.0f, 2.0f };
		const glm::vec3 target{ 0.0f, 0.0f, 0.0f };
		const glm::vec3 forward = glm::normalize(target - eye);
		const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
		const glm::mat4 farCube = glm::scale(glm::translate(glm::mat4(1.0f), target + 2.5f * forward + 0.9f * right), glm::vec3(1.5f));

		std::vector<DrawItem> drawItems{
			{
				.Mesh = meshAsset->GetGPUHandle(),
				.Texture = textureAsset->GetGPUHandle(),
				.Sampler = textureAsset->GetTextureData().Sampler,
				.TransformIndex = 0
			},
			{
				.Mesh = meshAsset->GetGPUHandle(),
				.Texture = {},		// The default handle resolves to the flat magenta error texture
				.Sampler = SamplerType::LinearWrap,
				.TransformIndex = 1
			}
		};
		std::vector<glm::mat4> transforms{ glm::mat4(1.0f), farCube };
		glm::mat4 viewMatrix = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
		//

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
				// Spin the cube about a tilted axis so every face takes a turn in front; rebuilt from total time, never accumulated
				const float angle = glm::radians(45.0f) * static_cast<float>(m_FrameStats.TotalTime);
				transforms[0] = glm::rotate(glm::mat4(1.0f), angle, glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f)));

				const bool minimized = m_Window->IsMinimized();
				const glm::uvec2 surfaceSize = m_Window->GetSize();
				const float aspect = surfaceSize.y > 0 ? static_cast<float>(surfaceSize.x) / static_cast<float>(surfaceSize.y) : 1.0f;

				CDL_PROFILE_SCOPE("Application::Run::FramePacket");
				FramePacket& packet = m_RenderManager->BeginFramePacket();
				CDL_PROFILE_SCOPE_METADATA(packet.FrameIndex);
				packet.DrawItems = drawItems;
				packet.TransformData = transforms;
				packet.ViewMatrix = viewMatrix;
				packet.ProjectionMatrix = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
				packet.Time = m_FrameStats.TotalTime;
				packet.Minimized = minimized;
				packet.SurfaceSize = surfaceSize;
				m_RenderManager->SubmitFramePacket(packet);

				// Nothing is presented while minimized, so nothing else holds the loop back
				if (minimized)
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
