#pragma once

#include "Core.h"
#include "Window.h"
#include "LayerStack.h"
#include "CAndle/Events/Event.h"
#include "Candle/Events/ApplicationEvent.h"

#include "Candle/ImGui/ImGuiLayer.h"

#include "Candle/Renderer/VertexArray.h"
#include "Candle/Renderer/Buffer.h"
#include "Candle/Renderer/Shader.h"

#include "Candle/Renderer/OrthographicCamera.h"

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

		std::shared_ptr<Shader> _shader;
		std::shared_ptr<Shader> _squareShader;

		std::shared_ptr<VertexArray> _vertexArray;
		std::shared_ptr<VertexArray> _squareVertexArray;

		OrthographicCamera _camera;
	};

	// To be defined in the client
	Application* CreateApplication();
}
