#include <Candle.h>

#include <glm/gtc/matrix_transform.hpp>

class ExampleLayer : public Candle::Layer
{
public:

	ExampleLayer()
		: Layer("Example"), _camera(-1.6f, 1.6f, -0.9f, 0.9f), _cameraPosition(0.0f)
	{
		// Triangle
		_vertexArray.reset(Candle::VertexArray::Create());

		float vertices[3 * 7] = {
			-0.5f, -0.5f, 0.0f,    1.0f, 0.0f, 0.0f, 1.0f,
			 0.5f, -0.5f, 0.0f,    0.0f, 1.0f, 0.0f, 1.0f,
			 0.0f,  0.5f, 0.0f,    0.0f, 0.0f, 1.0f, 1.0f,
		};

		std::shared_ptr<Candle::VertexBuffer> triangleVB;
		triangleVB.reset(Candle::VertexBuffer::Create(vertices, sizeof(vertices)));

		Candle::BufferLayout layout = {
			{ Candle::ShaderDataType::Float3, "vertexPos" },
			{ Candle::ShaderDataType::Float4, "vertexColor", true }
		};

		triangleVB->SetLayout(layout);
		_vertexArray->AddVertexBuffer(triangleVB);

		uint32_t indices[3] = {
			0, 1, 2
		};
		std::shared_ptr<Candle::IndexBuffer> triangleIB;
		triangleIB.reset(Candle::IndexBuffer::Create(indices, 3));
		_vertexArray->AddIndexBuffer(triangleIB);

		// Square
		_squareVertexArray.reset(Candle::VertexArray::Create());

		float vertices2[3 * 4] = {
			 0.5f, -0.5f, 0.0f,
			 0.5f,  0.5f, 0.0f,
			-0.5f,  0.5f, 0.0f,
			-0.5f, -0.5f, 0.0f
		};

		std::shared_ptr<Candle::VertexBuffer> squareVB;
		squareVB.reset(Candle::VertexBuffer::Create(vertices2, sizeof(vertices)));

		squareVB->SetLayout({
			{ Candle::ShaderDataType::Float3, "vertexPos" },
			});

		_squareVertexArray->AddVertexBuffer(squareVB);

		uint32_t indices2[6] = {
			0, 1, 2,
			2, 3, 0
		};

		std::shared_ptr<Candle::IndexBuffer> squareIB;
		squareIB.reset(Candle::IndexBuffer::Create(indices2, 6));
		_squareVertexArray->AddIndexBuffer(squareIB);

		std::string vertexSrc = R"(
			#version 330 core

			layout(location=0) in vec3 vertexPos;
			layout(location=1) in vec4 vertexColor;

			uniform mat4 transform;
			uniform mat4 viewProjectionMatrix;

			out vec3 fragmentPos;
			out vec4 fragmentColor;

			void main()
			{
				gl_Position = viewProjectionMatrix * transform * vec4(vertexPos, 1.0);

				fragmentPos = vertexPos;
				fragmentColor = vertexColor;
			}
		)";

		std::string fragmentSrc = R"(
			#version 330 core

			layout(location = 0) out vec4 color;

			in vec3 fragmentPos;
			in vec4 fragmentColor;

			void main()
			{
				color = fragmentColor;
			}
		)";

		_shader.reset(Candle::Shader::Create(vertexSrc, fragmentSrc));


		std::string flatColorShaderVertexSrc = R"(
			#version 330 core

			layout(location=0) in vec3 vertexPos;

			uniform mat4 transform;
			uniform mat4 viewProjectionMatrix;

			uniform vec4 u_Color;

			out vec3 fragmentPos;
			out vec4 fragmentColor;

			void main()
			{
				gl_Position = viewProjectionMatrix * transform * vec4(vertexPos, 1.0);

				fragmentPos = vertexPos;
				fragmentColor = u_Color;
			}
		)";

		std::string flatColorFragmentSrc = R"(
			#version 330 core

			layout(location = 0) out vec4 color;

			in vec3 fragmentPos;
			in vec4 fragmentColor;

			void main()
			{
				color = fragmentColor;
			}
		)";

		_flatShader.reset(Candle::Shader::Create(flatColorShaderVertexSrc, flatColorFragmentSrc));
	}


	void OnUpdate(Candle::Timestep timestep) override
	{
		if (Candle::Input::IsKeyPressed(CANDLE_KEY_W))
		{
			_cameraPosition.y += _cameraMoveSpeed * timestep;
		}
		if (Candle::Input::IsKeyPressed(CANDLE_KEY_A))
		{
			_cameraPosition.x -= _cameraMoveSpeed * timestep;
		}
		if (Candle::Input::IsKeyPressed(CANDLE_KEY_S))
		{
			_cameraPosition.y -= _cameraMoveSpeed * timestep;
		}
		if (Candle::Input::IsKeyPressed(CANDLE_KEY_D))
		{
			_cameraPosition.x += _cameraMoveSpeed * timestep;
		}
		if (Candle::Input::IsKeyPressed(CANDLE_KEY_E))
		{
			_cameraRotation -= _cameraRotationSpeed * timestep;
		}
		if (Candle::Input::IsKeyPressed(CANDLE_KEY_Q))
		{
			_cameraRotation += _cameraRotationSpeed * timestep;
		}

		Candle::RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.2f, 1.0f });
		Candle::RenderCommand::Clear();

		_camera.SetPosition(_cameraPosition);
		_camera.SetRotation(_cameraRotation);

		Candle::Renderer::BeginScene(_camera);

		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

		glm::vec4 redColor(1.0f, 0.0f, 0.0f, 1.0f);
		glm::vec4 blueColor(0.0f, 0.0f, 1.0f, 1.0f);
		
		for (int i = 0; i < 15; i++)
		{
			for (int j = 0; j < 10; j++)
			{
				glm::vec3 pos(j * 0.11f, i * 0.11f, 0.0f);
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;
				if (i % 2 == 0 && j % 2 == 0)
					_flatShader->UploadUniformFloat4("u_Color", redColor);
				else
					_flatShader->UploadUniformFloat4("u_Color", blueColor);

				Candle::Renderer::Submit(_flatShader, _squareVertexArray, transform);

			}
		}

		Candle::Renderer::Submit(_shader, _vertexArray);

		Candle::Renderer::EndScene();
	}

	void OnImGuiRender() override
	{

	}

	void OnEvent(Candle::Event& e) override
	{
		Candle::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<Candle::KeyPressedEvent>(CANDLE_BIND_EVENT_FN(ExampleLayer::OnKeyPressedEvent));
	}

	bool OnKeyPressedEvent(Candle::KeyPressedEvent evt)
	{

		return false;
	}

private:
	std::shared_ptr<Candle::Shader> _shader;
	std::shared_ptr<Candle::Shader> _flatShader;

	std::shared_ptr<Candle::VertexArray> _vertexArray;
	std::shared_ptr<Candle::VertexArray> _squareVertexArray;

	Candle::OrthographicCamera _camera;
	glm::vec3 _cameraPosition;
	float _cameraRotation = 0.0f;
	float _cameraMoveSpeed = 3.0f;
	float _cameraRotationSpeed = 90.0f;
};

class SandboxApp : public Candle::Application
{
public:
	SandboxApp()
	{
		PushLayer(new ExampleLayer());
	}

	~SandboxApp()
	{
	}
};

Candle::Application* Candle::CreateApplication()
{
	return new SandboxApp();
}