#include "candlepch.h"
#include "Renderer.h"

#include "Platform/OpenGL/OpenGLShader.h"

namespace Candle {

	Renderer::SceneData* Renderer::_sceneData = new Renderer::SceneData;

	void Renderer::BeginScene(OrthographicCamera& camera)
	{
		_sceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
	{
		vertexArray->Bind();
		shader->Bind();

		std::dynamic_pointer_cast<OpenGLShader>(shader)->UploadUniformMat4("transform", transform);
		std::dynamic_pointer_cast<OpenGLShader>(shader)->UploadUniformMat4("viewProjectionMatrix", _sceneData->ViewProjectionMatrix);

		RenderCommand::DrawIndexed(vertexArray);
	}

}