#include "candlepch.h"
#include "Application.h"
#include "Input.h"

#include "Platform/OpenGL/OpenGLBuffer.h"

#include <glad/glad.h>

namespace Candle {

	Application* Application::s_instance = nullptr;

	Application::Application()
	{
		CANDLE_CORE_ASSERT(!s_instance, "Application already exists!");
		s_instance = this;

		_window = std::unique_ptr<Window>(Window::Create());
		_window->SetEventCallback(CANDLE_BIND_EVENT_FN(Application::OnEvent));

		_imGuiLayer = new ImGuiLayer();
		PushOverlay(_imGuiLayer);

		// Triangle
		_vertexArray.reset(VertexArray::Create());

		float vertices[3 * 7] = {
			-0.5f, -0.5f, 0.0f,    1.0f, 0.0f, 0.0f, 1.0f,
			 0.5f, -0.5f, 0.0f,    0.0f, 1.0f, 0.0f, 1.0f,
			 0.0f,  0.5f, 0.0f,    0.0f, 0.0f, 1.0f, 1.0f,
		};

		std::shared_ptr<VertexBuffer> triangleVB;
		triangleVB.reset(VertexBuffer::Create(vertices, sizeof(vertices)));

		BufferLayout layout = {
			{ ShaderDataType::Float3, "vertexPos" },
			{ ShaderDataType::Float4, "vertexColor", true }
		};

		triangleVB->SetLayout(layout);
		_vertexArray->AddVertexBuffer(triangleVB);

		uint32_t indices[3] = {
			0, 1, 2
		};
		std::shared_ptr<IndexBuffer> triangleIB;
		triangleIB.reset(IndexBuffer::Create(indices, 3));
		_vertexArray->AddIndexBuffer(triangleIB);
		
		// Square
		_squareVertexArray.reset(VertexArray::Create());

		float vertices2[3 * 4] = {
			-0.75f, -0.75f, 0.0f,
			 0.75f, -0.75f, 0.0f,
			 0.75f,  0.75f, 0.0f,
			-0.75f,  0.75f, 0.0f
		};

		std::shared_ptr<VertexBuffer> squareVB;
		squareVB.reset(VertexBuffer::Create(vertices2, sizeof(vertices)));

		squareVB->SetLayout({
			{ ShaderDataType::Float3, "vertexPos" },
		});

		_squareVertexArray->AddVertexBuffer(squareVB);

		uint32_t indices2[6] = {
			0, 1, 2,
			2, 3, 0
		};

		std::shared_ptr<IndexBuffer> squareIB;
		squareIB.reset(IndexBuffer::Create(indices2, 6));
		_squareVertexArray->AddIndexBuffer(squareIB);

		std::string vertexSrc = R"(
			#version 330 core

			layout(location=0) in vec3 vertexPos;
			layout(location=1) in vec4 vertexColor;

			out vec3 fragmentPos;
			out vec4 fragmentColor;

			void main()
			{
				gl_Position = vec4(vertexPos, 1.0);

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

		_shader.reset(Shader::Create(vertexSrc, fragmentSrc));


		std::string squareVertexSrc = R"(
			#version 330 core

			layout(location=0) in vec3 vertexPos;

			out vec3 fragmentPos;

			void main()
			{
				gl_Position = vec4(vertexPos, 1.0);

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

		_squareShader.reset(Shader::Create(squareVertexSrc, squareFragmentSrc));
	}

	Application::~Application()
	{
	}

	void Application::PushLayer(Layer* layer)
	{
		_layerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* overlay)
	{
		_layerStack.PushOverlay(overlay);
		overlay->OnAttach();
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(CANDLE_BIND_EVENT_FN(Application::OnWindowClose));

		for (auto it = _layerStack.end(); it != _layerStack.begin(); ) 
		{
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}

	void Application::Run()
	{
		while (_isRunning) 
		{
			glClearColor(0, 0, 0.2f, 1);
			glClear(GL_COLOR_BUFFER_BIT);

			_squareShader->Bind();
			_squareVertexArray->Bind();

			glDrawElements(GL_TRIANGLES, _squareVertexArray->GetIndexBuffers()->GetCount(), GL_UNSIGNED_INT, nullptr);

			_squareVertexArray->Unbind();
			_squareShader->Unbind();


			_shader->Bind();
			_vertexArray->Bind();

			glDrawElements(GL_TRIANGLES, _vertexArray->GetIndexBuffers()->GetCount(), GL_UNSIGNED_INT, nullptr);

			_vertexArray->Unbind();
			_shader->Unbind();

			// Update layers
			for (Layer* layer : _layerStack)
				layer->OnUpdate();
			
			// Im gui render
			_imGuiLayer->Begin();

			for (Layer* layer : _layerStack)
				layer->OnImGuiRender();

			_imGuiLayer->End();

			// Finally update window
			_window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		_isRunning = false;
		return true;
	}
}