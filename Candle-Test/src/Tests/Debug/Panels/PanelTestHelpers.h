#pragma once

// Drives debug panels through a real ImGui context with no window, GPU or font upload, so their tests run headless.
// Panels keep their logic private, so what they draw is how a test sees it.

#include "TestFramework.h"
#include "TestHelpers.h"

#include <Candle/Debug/DebugContext.h>

#include <imgui.h>

#include <string>
#include <utility>

namespace Candle::Test {

	// Owns the ImGui context for one test, where GuiSystem would otherwise. Test-body thread only.
	class HeadlessGui
	{
	public:
		HeadlessGui()
		{
			ImGui::CreateContext();

			ImGuiIO& io = ImGui::GetIO();
			io.IniFilename = nullptr;
			io.DisplaySize = { 1280.0f, 720.0f };
			io.DeltaTime = 1.0f / 60.0f;
			io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;	// Lets ImGui build its font atlas on demand; nothing ever uploads it

			ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
			platformIO.Platform_ClipboardUserData = &m_Captured;
			platformIO.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
				*static_cast<std::string*>(ImGui::GetPlatformIO().Platform_ClipboardUserData) = text;
			};
		}

		~HeadlessGui() { ImGui::DestroyContext(); }

		HeadlessGui(const HeadlessGui&) = delete;
		HeadlessGui& operator=(const HeadlessGui&) = delete;

		template<typename Fn>
		void Frame(Fn&& draw)
		{
			ImGui::NewFrame();
			draw();
			ImGui::Render();
		}

		// The text of every item draw submits, scrolled out of view or not. This is ImGui's log, so text added straight to a
		// draw list isn't in it.
		template<typename Fn>
		std::string Capture(Fn&& draw)
		{
			Frame([&] {
				ImGui::LogToClipboard();
				draw();
				ImGui::LogFinish();
			});
			return std::exchange(m_Captured, {});
		}

		// A full click at pos: the frames to hover, press and release, since ImGui reports each one a frame apart.
		template<typename Fn>
		void Click(ImVec2 pos, Fn&& draw)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.AddMousePosEvent(pos.x, pos.y);
			Frame(draw);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			Frame(draw);
			io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			Frame(draw);
		}

	private:
		std::string m_Captured;
	};

}
