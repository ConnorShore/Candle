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
		glGenBuffers(1, &_vertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);

		float vertices[9] = {
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
			 0.0f,  0.5f, 0.0f
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

		glGenBuffers(1, &_indexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBuffer);

		unsigned int indices[3] = {
			0, 1, 2
		};

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
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

			glBindVertexArray(_vertexArray);
			glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);

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