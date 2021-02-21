#include "candlepch.h"
#include "Renderer.h"

namespace Candle {

	Renderer::SceneData* Renderer::_sceneData = new Renderer::SceneData;

	void Renderer::BeginScene(OrthographicCamera& camera)
	{
		_sceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Submit(const std::shared_ptr<Shader>& shader, const std::shared_ptr<VertexArray>& vertexArray, const glm::mat4& transform)
	{
		vertexArray->Bind();
		shader->Bind();

		shader->UploadUniformMat4("transform", transform);
		shader->UploadUniformMat4("viewProjectionMatrix", _sceneData->ViewProjectionMatrix);

		RenderCommand::DrawIndexed(vertexArray);

		shader->Unbind();
		vertexArray->Unbind();
	}

}