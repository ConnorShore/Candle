#pragma once

#include "Candle/Core.h"
#include "Candle/Window.h"
#include "Candle/LayerStack.h"
#include "Candle/Events/Event.h"
#include "Candle/Core/Timestep.h"
#include "Candle/Events/ApplicationEvent.h"

#include "Candle/ImGui/ImGuiLayer.h"


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
		Scope<Window> _window;

		bool _isRunning = true;
		LayerStack _layerStack;

		ImGuiLayer* _imGuiLayer;

		float _lastFrameTime = 0.0f;

	private:
		static Application* s_instance;
	};

	// To be defined in the client
	Application* CreateApplication();
}
