#include "candlepch.h"
#include "Application.h"
#include "Input.h"

#include "Candle/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLBuffer.h"

#include <glfw/glfw3.h>


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
			float time = (float)glfwGetTime();	// TODO: Platform::GetTime()
			Timestep timestep = time - _lastFrameTime;
			_lastFrameTime = time;

			// Update layers
			for (Layer* layer : _layerStack)
				layer->OnUpdate(timestep);
			
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