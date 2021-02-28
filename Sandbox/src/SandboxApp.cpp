#include <Candle.h>

#include <Platform/OpenGL/OpenGLShader.h>

#include <imgui/imgui.cpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtc/type_ptr.hpp>

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

		Candle::Ref<Candle::VertexBuffer> triangleVB;
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
		Candle::Ref<Candle::IndexBuffer> triangleIB;
		triangleIB.reset(Candle::IndexBuffer::Create(indices, 3));
		_vertexArray->AddIndexBuffer(triangleIB);

		// Square
		_squareVertexArray.reset(Candle::VertexArray::Create());

		float vertices2[4 * 5] = {
			 0.5f, -0.5f, 0.0f,		1.0f, 0.0f,
			 0.5f,  0.5f, 0.0f,		1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f,		0.0f, 1.0f,
			-0.5f, -0.5f, 0.0f,		0.0f, 0.0f
		};

		Candle::Ref<Candle::VertexBuffer> squareVB;
		squareVB.reset(Candle::VertexBuffer::Create(vertices2, sizeof(vertices)));

		squareVB->SetLayout({
			{ Candle::ShaderDataType::Float3, "vertexPos" },
			{ Candle::ShaderDataType::Float2, "vertexUV" },
		});

		_squareVertexArray->AddVertexBuffer(squareVB);

		uint32_t indices2[6] = {
			0, 1, 2,
			2, 3, 0
		};

		Candle::Ref<Candle::IndexBuffer> squareIB;
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

		_shader = Candle::Shader::Create("VertexPosColor", vertexSrc, fragmentSrc);


		std::string flatColorShaderVertexSrc = R"(
			#version 330 core

			layout(location=0) in vec3 vertexPos;

			uniform mat4 transform;
			uniform mat4 viewProjectionMatrix;

			out vec3 fragmentPos;

			void main()
			{
				gl_Position = viewProjectionMatrix * transform * vec4(vertexPos, 1.0);

				fragmentPos = vertexPos;
			}
		)";

		std::string flatColorFragmentSrc = R"(
			#version 330 core

			layout(location = 0) out vec4 color;

			in vec3 fragmentPos;

			uniform vec3 u_Color;

			void main()
			{
				color = vec4(u_Color, 1.0);
			}
		)";

		_flatShader = Candle::Shader::Create("FlatColor", flatColorShaderVertexSrc, flatColorFragmentSrc);

		auto textureShader = _shaderLibrary.Load("assets/shaders/Texture.glsl");

		_texture = Candle::Texture2D::Create("assets/textures/Checkerboard.png");
		_logoTexture = Candle::Texture2D::Create("assets/textures/ChernoLogo.png");

		textureShader->Bind();
		std::dynamic_pointer_cast<Candle::OpenGLShader>(textureShader)->UploadUniformInt("texture2D", 0);
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

		_flatShader->Bind();
		std::dynamic_pointer_cast<Candle::OpenGLShader>(_flatShader)->UploadUniformFloat3("u_Color", _squareColor);
		
		// Square grid
		for (int i = 0; i < 15; i++)
		{
			for (int j = 0; j < 10; j++)
			{
				glm::vec3 pos(j * 0.11f, i * 0.11f, 0.0f);
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;

				Candle::Renderer::Submit(_flatShader, _squareVertexArray, transform);
			}
		}

		// Texture square
		auto textureShader = _shaderLibrary.Get("Texture");
		_texture->Bind(0);
		Candle::Renderer::Submit(textureShader, _squareVertexArray, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));

		// Texture square
		_logoTexture->Bind(0);
		Candle::Renderer::Submit(textureShader, _squareVertexArray, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));

		// Triangle
		//Candle::Renderer::Submit(_shader, _vertexArray);

		Candle::Renderer::EndScene();
	}

	void OnImGuiRender() override
	{
		ImGui::Begin("Settings");

		ImGui::ColorEdit3("Square Color", glm::value_ptr(_squareColor));

		ImGui::End();
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
	Candle::ShaderLibrary _shaderLibrary;

	Candle::Ref<Candle::Shader> _shader;
	Candle::Ref<Candle::Shader> _flatShader;

	Candle::Ref<Candle::Texture2D> _texture, _logoTexture;

	Candle::Ref<Candle::VertexArray> _vertexArray;
	Candle::Ref<Candle::VertexArray> _squareVertexArray;

	Candle::OrthographicCamera _camera;
	glm::vec3 _cameraPosition;

	float _cameraRotation = 0.0f;
	float _cameraMoveSpeed = 3.0f;
	float _cameraRotationSpeed = 90.0f;

	glm::vec3 _squareColor = { 0.2f, 0.3f, 0.85f };
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