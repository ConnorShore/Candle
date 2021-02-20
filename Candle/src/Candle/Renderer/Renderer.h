#pragma once

#include "Candle/Renderer/RenderCommand.h"
#include "Candle/Renderer/OrthographicCamera.h"
#include "Candle/Renderer/Shader.h"

namespace Candle {

	class CANDLE_API Renderer
	{
	public:

		static void BeginScene(OrthographicCamera& camera);	// TODO: Scene parameters
		static void EndScene();

		static void Submit(const std::shared_ptr<Shader>& shader, const std::shared_ptr<VertexArray>& vertexArray);

		inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

	private:
		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};

		static SceneData* _sceneData;
	};
}