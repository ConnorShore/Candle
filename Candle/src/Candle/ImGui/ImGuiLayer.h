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

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		virtual void OnImGuiRender() override;

		void Begin();
		void End();

	private:
		float _time = 0.0f;
	};
}