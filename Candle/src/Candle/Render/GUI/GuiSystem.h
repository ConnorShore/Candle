#pragma once

#include "GuiDrawData.h"

struct ImDrawData;
union SDL_Event;

namespace Candle {

	class Window;

	// A system that manages the GUI layer, including ImGui context and event handling.
	// Threading: main thread only. The GUI system must be created and destroyed on the main thread, and all its methods must be called from the main thread.
	class GuiSystem
	{
	public:
		GuiSystem(Window& window);
		~GuiSystem();

		bool WantsInput(const InputEvent& event) const;

		void StartFrame();
		void EndFrame();
		GuiDrawData ExtractDrawData() const;

		void SetNavigationEnabled(bool enabled);
		inline bool IsNavigationEnabled() const { return m_NavigationEnabled; }
		
	private:
		static void OnPlatformEvent(const SDL_Event& event, void* userData);

	private:
		bool m_NavigationEnabled{ true };
		uint8_t m_NavigationFlags{ 0 };
	};

}