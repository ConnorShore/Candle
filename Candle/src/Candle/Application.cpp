#include "candlepch.h"
#include "Application.h"
#include "Input.h"

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

		// Create vertex array
		glGenVertexArrays(1, &_vertexArray);
		glBindVertexArray(_vertexArray);

		// Create vertex buffer
		float vertices[9] = {
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
			 0.0f,  0.5f, 0.0f
		};

		_vertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

		uint32_t indices[3] = {
			0, 1, 2
		};

		_indexBuffer.reset(IndexBuffer::Create(indices, 3));

		std::string vertexSrc = R"(
			#version 330 core

			layout(location=0) in vec3 vertexPos;

			out vec3 fragmentPos;

			void main()
			{
				fragmentPos = vertexPos;
				gl_Position = vec4(vertexPos, 1.0);
			}
		)";

		std::string fragmentSrc = R"(
			#version 330 core

			in vec3 fragmentPos;

			out vec4 color;

			void main()
			{
				color = vec4(fragmentPos * 0.25 + 0.5, 1.0);
			}
		)";

		_shader.reset(Shader::Create(vertexSrc, fragmentSrc));
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

			_shader->Bind();

			glBindVertexArray(_vertexArray);
			glDrawElements(GL_TRIANGLES, _indexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);

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