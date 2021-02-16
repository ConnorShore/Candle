#pragma once

#include "Core.h"
#include "Window.h"
#include "LayerStack.h"
#include "CAndle/Events/Event.h"
#include "Candle/Events/ApplicationEvent.h"

#include "Candle/ImGui/ImGuiLayer.h"

#include "Candle/Renderer/Shader.h"

namespace Candle {

	class CANDLE_API Application
	{
	public:
		Application();
		virtual ~Application();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);

		void OnEvent(Event& e);
		void Run();

		inline Window& GetWindow() { return *_window; }
		inline static Application& Get() { return *s_instance; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);

	private:
		std::unique_ptr<Window> _window;
		bool _isRunning = true;
		LayerStack _layerStack;

		ImGuiLayer* _imGuiLayer;

		static Application* s_instance;

		unsigned int _vertexArray, _vertexBuffer, _indexBuffer;

		std::unique_ptr<Shader> _shader;
	};

	// To be defined in the client
	Application* CreateApplication();
}
