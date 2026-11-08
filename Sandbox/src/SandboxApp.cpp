#include "SandboxApp.h"

#include <Candle/Core/Job/JobSystem.h>
#include <Candle/Render/Assets/MeshAsset.h>
#include <Candle/Render/Assets/Texture.h>

#include <imgui.h>

#include <cmath>

namespace Candle {

	SandboxApp::SandboxApp(const ApplicationSpecification& spec)
		: Application(spec)
	{

	}

	void SandboxApp::OnInit(ApplicationContext context)
	{
		// TEMPORARY FOR TESTING
		MeshData testMeshData{
			.Vertices = {
				// +Z
				{ { -0.5f, -0.5f,  0.5f }, 0.0f,{ 0.0f,  0.0f,  1.0f }, 0.0f },
				{ { 0.5f, -0.5f,  0.5f }, 1.0f,{ 0.0f,  0.0f,  1.0f }, 0.0f },
				{ { 0.5f,  0.5f,  0.5f }, 1.0f,{ 0.0f,  0.0f,  1.0f }, 1.0f },
				{ { -0.5f,  0.5f,  0.5f }, 0.0f,{ 0.0f,  0.0f,  1.0f }, 1.0f },
				// -Z
				{ { 0.5f, -0.5f, -0.5f }, 0.0f,{ 0.0f,  0.0f, -1.0f }, 0.0f },
				{ { -0.5f, -0.5f, -0.5f }, 1.0f,{ 0.0f,  0.0f, -1.0f }, 0.0f },
				{ { -0.5f,  0.5f, -0.5f }, 1.0f,{ 0.0f,  0.0f, -1.0f }, 1.0f },
				{ { 0.5f,  0.5f, -0.5f }, 0.0f,{ 0.0f,  0.0f, -1.0f }, 1.0f },
				// +X
				{ { 0.5f, -0.5f,  0.5f }, 0.0f,{ 1.0f,  0.0f,  0.0f }, 0.0f },
				{ { 0.5f, -0.5f, -0.5f }, 1.0f,{ 1.0f,  0.0f,  0.0f }, 0.0f },
				{ { 0.5f,  0.5f, -0.5f }, 1.0f,{ 1.0f,  0.0f,  0.0f }, 1.0f },
				{ { 0.5f,  0.5f,  0.5f }, 0.0f,{ 1.0f,  0.0f,  0.0f }, 1.0f },
				// -X
				{ { -0.5f, -0.5f, -0.5f }, 0.0f,{ -1.0f,  0.0f,  0.0f }, 0.0f },
				{ { -0.5f, -0.5f,  0.5f }, 1.0f,{ -1.0f,  0.0f,  0.0f }, 0.0f },
				{ { -0.5f,  0.5f,  0.5f }, 1.0f,{ -1.0f,  0.0f,  0.0f }, 1.0f },
				{ { -0.5f,  0.5f, -0.5f }, 0.0f,{ -1.0f,  0.0f,  0.0f }, 1.0f },
				// +Y
				{ { -0.5f,  0.5f,  0.5f }, 0.0f,{ 0.0f,  1.0f,  0.0f }, 0.0f },
				{ { 0.5f,  0.5f,  0.5f }, 1.0f,{ 0.0f,  1.0f,  0.0f }, 0.0f },
				{ { 0.5f,  0.5f, -0.5f }, 1.0f,{ 0.0f,  1.0f,  0.0f }, 1.0f },
				{ { -0.5f,  0.5f, -0.5f }, 0.0f,{ 0.0f,  1.0f,  0.0f }, 1.0f },
				// -Y
				{ { -0.5f, -0.5f, -0.5f }, 0.0f,{ 0.0f, -1.0f,  0.0f }, 0.0f },
				{ { 0.5f, -0.5f, -0.5f }, 1.0f,{ 0.0f, -1.0f,  0.0f }, 0.0f },
				{ { 0.5f, -0.5f,  0.5f }, 1.0f,{ 0.0f, -1.0f,  0.0f }, 1.0f },
				{ { -0.5f, -0.5f,  0.5f }, 0.0f,{ 0.0f, -1.0f,  0.0f }, 1.0f }
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
		uint64_t meshId = context.AssetManager.LoadAsset(meshAsset);

		TextureData data{
			.Sampler{ SamplerType::LinearWrap }
		};
		SharedPtr<Texture> textureAsset = SharedPtr<Texture>::Create("TestTexture", GetSpecification().EngineAssetDir / "textures/box.jpg", std::move(data));
		uint64_t textureId = context.AssetManager.LoadAsset(textureAsset);

		const glm::vec3 texturedCubePosition{ 0.0f, 0.0f, 0.0f };
		const glm::vec3 magentaCubePosition{ -0.8f, -1.4f, -2.1f };

		// Textured cube
		std::vector<Entity> entities;
		entities.push_back({
			.MeshID = meshId,
			.TextureID = textureId,
			.Transform = glm::translate(glm::mat4(1.0f), texturedCubePosition)
			});
		// Magenta cube
		entities.push_back({
			.MeshID = meshId,
			.TextureID = 0,	// No texture, fallback to error texture
			.Transform = glm::scale(glm::translate(glm::mat4(1.0f), magentaCubePosition), glm::vec3(1.5f))
			});

		// Set up the world with the entities
		context.ActiveWorld.Entities = entities;

		m_CameraPose.Orientation = glm::quatLookAt(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		CDL_INFO(LogChannel::Application, "SandboxApp initialized!");
	}

	void SandboxApp::OnUpdate(UpdateContext context)
	{
		// Update textured cube transform to spin it
		Entity& cubeEntity = context.AppContext.ActiveWorld.Entities[0];
		cubeEntity.Transform = glm::rotate(cubeEntity.Transform, glm::radians(45.0f) * context.DeltaTime.GetSeconds(), glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f)));

		// Match the camera to the window's shape, or the image stretches
		// TODO: Fix output size in future, this currently doesn't sync with swapchain extent, so the aspect ratio is wrong for the camera if the window is resized
		const glm::uvec2 outputSize = context.Render.GetOutputSize();
		m_CameraPose = UpdateSimpleController(m_CameraController, m_CameraPose, context.Input, context.DeltaTime);
		context.AppContext.ActiveWorld.RenderView = MakeRenderView(m_CameraPose, m_CameraLens, static_cast<float>(outputSize.x) / outputSize.y);

		static bool drawn = false;
		if (!drawn)
		{
			DebugDraw::Cube(glm::vec3(2.0f, 2.0f, 0.0f), glm::vec3(1.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f), { .Duration{ 3.0f }, .DepthTest{ true } });
			DebugDraw::Cube(glm::vec3(-2.0f, 2.0f, 0.0f), glm::vec3(1.0f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f), { .Duration{ 1.5f }, .DepthTest{ true } });

			DebugDraw::Point(glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), { .Duration{ 5.0f }, .DepthTest{ false } });
			DebugDraw::Point(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), { .Duration{ 5.0f }, .DepthTest{ false } });
			DebugDraw::Line(glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), { .Duration{ 5.0f }, .DepthTest{ false } });

			DebugDraw::Circle(glm::vec3(0.0f, -2.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), 1.5f, glm::vec4(1.0f), { .Duration{ DebugDraw::kUnlimitedDuration }, .DepthTest{ true } });

			drawn = true;
		}

		// Show the world in a window when it renders to a texture
		if (!context.Render.IsRenderingToSwapChain())
		{
			ImGui::DockSpaceOverViewport();

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("Scene");
			const ImVec2 size = ImGui::GetContentRegionAvail();
			context.Render.SetOutputSize({ size.x, size.y });
			ImGui::Image(context.Render.GetOutputTextureID(), size);
			ImGui::End();
			ImGui::PopStyleVar();
		}

		//ImGui::ShowDemoWindow();
	}

	void SandboxApp::OnShutdown()
	{
		CDL_INFO(LogChannel::Application, "SandboxApp shutdown!");
	}

}