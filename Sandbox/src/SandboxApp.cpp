#include <Candle.h>

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
			-0.75f, -0.75f, 0.0f,
			 0.75f, -0.75f, 0.0f,
			 0.75f,  0.75f, 0.0f,
			-0.75f,  0.75f, 0.0f
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

			uniform mat4 viewProjectionMatrix;

			out vec3 fragmentPos;
			out vec4 fragmentColor;

			void main()
			{
				gl_Position = viewProjectionMatrix * vec4(vertexPos, 1.0);

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


		std::string squareVertexSrc = R"(
			#version 330 core

			layout(location=0) in vec3 vertexPos;

			uniform mat4 viewProjectionMatrix;

			out vec3 fragmentPos;

			void main()
			{
				gl_Position = viewProjectionMatrix * vec4(vertexPos, 1.0);

				fragmentPos = vertexPos;
			}
		)";

		std::string squareFragmentSrc = R"(
			#version 330 core

			layout(location = 0) out vec4 color;

			in vec3 fragmentPos;

			void main()
			{
				color = vec4(0.2, 0.3, 0.85, 1.0);
			}
		)";

		_squareShader.reset(Candle::Shader::Create(squareVertexSrc, squareFragmentSrc));
	}


	void OnUpdate(Candle::Timestep timestep) override
	{
		CANDLE_TRACE("Delta Time: {0} s [{1} ms]", timestep, timestep.GetMilliseconds());

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

		Candle::Renderer::Submit(_squareShader, _squareVertexArray);
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
	std::shared_ptr<Candle::Shader> _squareShader;

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