#pragma once

#include "Candle/Core.h"
#include "Candle/Layer.h"
#include "Candle/Events/ApplicationEvent.h"
#include "Candle/Events/KeyEvent.h"
#include "Candle/Events/MouseEvent.h"

namespace Candle {

	class CANDLE_API ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate() override;
		void OnEvent(Event& event) override;

	private:
		bool OnMouseButtonPressedEvent(MouseButtonPressedEvent& e);
		bool OnMouseButtonReleasedEvent(MouseButtonReleasedEvent& e);
		bool OnMouseMovedEvent(MouseMovedEvent& e);
		bool OnMouseScrolledvent(MouseScrolledEvent& e);
		bool OnKeyTypedEvent(KeyTypedEvent& e);
		bool OnKeyPressedEvent(KeyPressedEvent& e);
		bool OnKeyReleasedEvent(KeyReleasedEvent& e);
		bool OnWindowResizeEvent(WindowResizeEvent& e);

	private:
		float _time = 0.0f;
	};
}