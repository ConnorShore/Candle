#include "cdlpch.h"
#include "GuiSystem.h"
#include "Candle/Platform/Window.h"
#include "Candle/Platform/SDL/SDLPlatform.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl3.h>

namespace Candle {

	namespace {

		GPUTextureHandle ToTextureHandle(ImTextureID id)
		{
			if (id == 0)
				return GPUTextureHandle{ 0 };
			return GPUTextureHandle{ reinterpret_cast<uint64_t>(id) };
		}

	}

	GuiSystem::GuiSystem(Window& window)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GuiSystem::GuiSystem must be called on the main thread");

		// Ensure that our GuiVertex and GuiIndex types match ImGui's types, so we can safely reinterpret_cast them when copying data to the GPU.
		CDL_STATIC_ASSERT(sizeof(GuiVertex) == sizeof(ImDrawVert));
		CDL_STATIC_ASSERT(offsetof(GuiVertex, Position) == offsetof(ImDrawVert, pos));
		CDL_STATIC_ASSERT(offsetof(GuiVertex, UV) == offsetof(ImDrawVert, uv));
		CDL_STATIC_ASSERT(offsetof(GuiVertex, Color) == offsetof(ImDrawVert, col));
		CDL_STATIC_ASSERT(sizeof(GuiIndex) == sizeof(ImDrawIdx));

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;        // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking

		// TODO: Add support in engine for Multi-Viewport / Platform Windows
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;

		// Remember which nav flags were asked for so SetNavigationEnabled can restore exactly those.
		m_NavigationFlags = io.ConfigFlags & (ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);
		m_NavigationEnabled = true;

		ImGui::StyleColorsDark();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// Setup scaling
		float scaleing = window.GetContentScale();
		style.ScaleAllSizes(scaleing);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
		style.FontScaleDpi = scaleing;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

		ImGui_ImplSDL3_InitForVulkan(reinterpret_cast<SDL_Window*>(window.GetNativeHandle()));
		SDL::SetRawEventHook(&GuiSystem::OnPlatformEvent, this);
	}

	GuiSystem::~GuiSystem()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GuiSystem::~GuiSystem must be called on the main thread");
		SDL::SetRawEventHook(nullptr, nullptr);
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}

	bool GuiSystem::WantsInput(const InputEvent& event) const
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GuiSystem::WantsInput must be called on the main thread");
		return (IsMouseEvent(event) && ImGui::GetIO().WantCaptureMouse) || (IsKeyEvent(event) && ImGui::GetIO().WantCaptureKeyboard);
	}

	void GuiSystem::StartFrame()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GuiSystem::StartFrame must be called on the main thread");
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
	}

	void GuiSystem::EndFrame()
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GuiSystem::EndFrame must be called on the main thread");
		ImGui::Render();
	}

	void GuiSystem::SetNavigationEnabled(bool enabled)
	{
		if (m_NavigationEnabled == enabled)
			return;

		m_NavigationEnabled = enabled;

		ImGuiIO& io = ImGui::GetIO();
		if (enabled)
		{
			io.ConfigFlags |= m_NavigationFlags;
		}
		else
		{
			io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);

			// Drop the nav highlight and any active nav item so the last-focused widget doesn't
			// stay lit up (or keep swallowing keys) for the whole play session.
			ImGui::SetNavCursorVisible(false);
			ImGui::FocusWindow(nullptr);
		}
	}

	void GuiSystem::OnPlatformEvent(const SDL_Event& event, void* userData)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "GuiSystem::OnPlatformEvent must be called on the main thread");
		ImGui_ImplSDL3_ProcessEvent(&event);
	}

	GuiDrawData GuiSystem::ExtractDrawData() const
	{
		const ImDrawData* drawData = ImGui::GetDrawData();
		GuiDrawData out{
				.DisplayPos = { drawData->DisplayPos.x, drawData->DisplayPos.y },
				.DisplaySize = { drawData->DisplaySize.x, drawData->DisplaySize.y }
		};
		out.Vertices.resize(drawData->TotalVtxCount);
		out.Indices.resize(drawData->TotalIdxCount);

		const ImVec2 pos = drawData->DisplayPos;
		const ImVec2 scale = drawData->FramebufferScale;
		uint32_t vertexBase = 0, indexBase = 0;
		for (const ImDrawList* list : drawData->CmdLists)
		{
			std::memcpy(out.Vertices.data() + vertexBase, list->VtxBuffer.Data, list->VtxBuffer.size_in_bytes());
			std::memcpy(out.Indices.data() + indexBase, list->IdxBuffer.Data, list->IdxBuffer.size_in_bytes());

			for (const ImDrawCmd& cmd : list->CmdBuffer)
			{
				if (cmd.UserCallback)   // TODO: Support user callbacks in the future if we need them
					continue;

				out.Commands.push_back({
						.ClipRect = { (cmd.ClipRect.x - pos.x) * scale.x, (cmd.ClipRect.y - pos.y) * scale.y,
										(cmd.ClipRect.z - pos.x) * scale.x, (cmd.ClipRect.w - pos.y) * scale.y },
						.Texture = ToTextureHandle(cmd.GetTexID()),
						.VertexOffset = vertexBase + cmd.VtxOffset,
						.IndexOffset = indexBase + cmd.IdxOffset,
						.IndexCount = cmd.ElemCount
					});
			}

			vertexBase += list->VtxBuffer.Size;
			indexBase += list->IdxBuffer.Size;
		}
		return out;
	}

}