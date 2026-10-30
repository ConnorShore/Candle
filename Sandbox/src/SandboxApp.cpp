#include <Candle.h>
#include <Candle/App/EntryPoint.h>
#include <Candle/Core/Job/JobSystem.h>
#include <Candle/Render/Assets/MeshAsset.h>
#include <Candle/Render/Assets/Texture.h>

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

		void OnInit(ApplicationContext context) override
		{
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
			uint64_t meshId = context.AssetManager.LoadAsset(meshAsset);

			TextureData data{
				.Sampler{SamplerType::LinearWrap}
			};
			SharedPtr<Texture> textureAsset = SharedPtr<Texture>::Create("TestTexture", GetSpecification().EngineAssetDir / "textures/box.jpg", std::move(data));
			uint64_t textureId = context.AssetManager.LoadAsset(textureAsset);

			const glm::vec3 eye{ 2.0f, 2.0f, 2.0f };
			const glm::vec3 target{ 0.0f, 0.0f, 0.0f };
			const glm::vec3 forward = glm::normalize(target - eye);
			const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
			const glm::mat4 farCube = glm::scale(glm::translate(glm::mat4(1.0f), target + 2.5f * forward + 0.9f * right), glm::vec3(1.5f));

			// Textured cube
			std::vector<Entity> entities;
			entities.push_back({
				.MeshID = meshId,
				.TextureID = textureId,
				.Transform = glm::mat4(1.0f)
				});
			// Magenta cube
			entities.push_back({
				.MeshID = meshId,
				.TextureID = 0,	// No texture, fallback to error texture
				.Transform = farCube
				});

			// Set up the world with a view, projection (temporary camera essentially) and the entities
			glm::mat4 viewMatrix = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
			context.ActiveWorld.View = viewMatrix;
			context.ActiveWorld.Projection = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 100.0f);
			context.ActiveWorld.Entities = entities;

			CDL_INFO(LogChannel::Application, "SandboxApp initialized!");
		}

		void OnUpdate(UpdateContext context) override
		{
			// Update textured cube transform to spin it
			Entity& cubeEntity = context.AppContext.ActiveWorld.Entities[0];
			cubeEntity.Transform = glm::rotate(cubeEntity.Transform, glm::radians(45.0f) * context.DeltaTime.GetSeconds(), glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f)));
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