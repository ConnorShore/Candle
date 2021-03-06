#pragma once

#include "Candle/Renderer/RenderCommand.h"
#include "Candle/Renderer/OrthographicCamera.h"
#include "Candle/Renderer/Shader.h"

namespace Candle {

	class CANDLE_API Renderer
	{
	public:
		
		static void Init();

		static void BeginScene(OrthographicCamera& camera);	// TODO: Scene parameters
		static void EndScene();

		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));

		inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

	private:
		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};

		static SceneData* _sceneData;
	};
}