// GuiSystem on a real window and texture pool: the ImGui context it owns, the textures ImGui asks it to upload and later
// release, the draw data it extracts for the render thread, the input it claims, and the SDL events it feeds ImGui. Each
// test fails on any validation message.

#include "TestFramework.h"
#include "TestHelpers.h"
#include "../RenderTestHelpers.h"

#include <Candle/Render/GUI/GuiDrawData.h>
#include <Candle/Render/GUI/GuiSystem.h>
#include <Candle/Render/GUI/GuiTextureID.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <optional>
#include <vector>

using namespace Candle;
using namespace Candle::Test;
using Candle::Test::Type::Gpu;

namespace {

	// What Application hands its GuiSystem: the window, and the texture pool ImGui's font atlas uploads into. Gui is
	// declared last, so it releases its textures before the pool goes.
	struct TestGui : TestTextures
	{
		Window AppWindow{ TestWindowSpec() };
		std::optional<GuiSystem> Gui;

		TestGui() { Start(); }

		void Start()
		{
			Gui.emplace(AppWindow, Pool);
			KeepImGuiOffDisk();
		}
	};

	// One GUI frame as Application::Run brackets OnUpdate, with draw making the application's ImGui calls.
	template<typename Fn>
	GuiDrawData RunGuiFrame(GuiSystem& gui, Fn&& draw)
	{
		gui.StartFrame();
		draw();
		gui.EndFrame();
		return gui.ExtractDrawData();
	}

	// Placed and sized up front: ImGui hides a new auto-sized window for its first frame while it measures it, so a test
	// window left to size itself draws nothing in the one frame a test runs.
	void BeginTestWindow(const char* name, ImVec2 pos = { 10.0f, 10.0f })
	{
		ImGui::SetNextWindowPos(pos);
		ImGui::SetNextWindowSize({ 200.0f, 100.0f });
		ImGui::Begin(name);
	}

	// Two windows, each its own draw list, so extraction has to offset the second list's vertices and indices.
	void DrawTwoWindows()
	{
		BeginTestWindow("First");
		ImGui::Text("Hello");
		ImGui::Button("Button");
		ImGui::End();

		BeginTestWindow("Second", { 220.0f, 10.0f });
		ImGui::Text("World");
		ImGui::End();
	}

	void NoOpCallback(const ImDrawList*, const ImDrawCmd*) {}

	// The commands ExtractDrawData keeps from ImGui's draw data: everything but user callbacks, in order.
	std::vector<std::pair<const ImDrawList*, const ImDrawCmd*>> DrawableCommands(const ImDrawData& drawData)
	{
		std::vector<std::pair<const ImDrawList*, const ImDrawCmd*>> commands;
		for (const ImDrawList* list : drawData.CmdLists)
			for (const ImDrawCmd& command : list->CmdBuffer)
				if (!command.UserCallback)
					commands.emplace_back(list, &command);
		return commands;
	}

	bool SameHandle(GPUTextureHandle a, GPUTextureHandle b) { return a.Index == b.Index && a.Generation == b.Generation; }

	std::vector<GPUTextureHandle> DistinctTextures(const GuiDrawData& gui)
	{
		std::vector<GPUTextureHandle> textures;
		for (const GuiDrawCommand& command : gui.Commands)
			if (std::ranges::none_of(textures, [&](GPUTextureHandle t) { return SameHandle(t, command.Texture); }))
				textures.push_back(command.Texture);
		return textures;
	}

}

//////////////////////////////////////////////////////////////////////////
// The ImGui context
//////////////////////////////////////////////////////////////////////////

// What the engine supports and turns on: docking and navigation, draw commands with vertex offsets so a list can pass 64K
// vertices with 16-bit indices, and texture uploads handled by the backend. The style is scaled to the window's display.
CDL_TEST_CASE(GuiSystem, ConfiguresImGuiForTheEngine, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		CDL_CHECK(ImGui::GetCurrentContext());
		const ImGuiIO& io = ImGui::GetIO();

		CDL_EXPECT(io.ConfigFlags & ImGuiConfigFlags_DockingEnable);
		CDL_EXPECT(io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard);
		CDL_EXPECT(io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad);
		CDL_EXPECT(io.BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset);
		CDL_EXPECT(io.BackendFlags & ImGuiBackendFlags_RendererHasTextures);
		CDL_EXPECT(io.BackendPlatformUserData != nullptr);	// The SDL3 platform backend is up
		CDL_EXPECT_EQ(ImGui::GetStyle().FontScaleDpi, test.AppWindow.GetContentScale());
		CDL_EXPECT(test.Gui->IsNavigationEnabled());
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Application builds one per run, and the next must find no context and no SDL event hook left behind, or
// SDL::SetRawEventHook's one-hook assert fires.
CDL_TEST_CASE(GuiSystem, CanBeRebuiltOnceDestroyed, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		RunGuiFrame(*test.Gui, DrawTwoWindows);

		test.Gui.reset();
		CDL_EXPECT(ImGui::GetCurrentContext() == nullptr);

		test.Start();
		CDL_CHECK(ImGui::GetCurrentContext());
		const GuiDrawData gui = RunGuiFrame(*test.Gui, DrawTwoWindows);
		CDL_EXPECT_FALSE(gui.Commands.empty());
		for (const GPUTextureHandle texture : DistinctTextures(gui))
			CDL_EXPECT(test.Pool.IsAlive(texture));
	}
	ExpectNoVulkanDebugMessages(logs);
}

//////////////////////////////////////////////////////////////////////////
// Textures
//////////////////////////////////////////////////////////////////////////

// With RendererHasTextures, ImGui asks the backend to create its font atlas in the first frame. It must land in the pool at
// ImGui's size, as a plain texture, and every command must reference a texture the pool still holds.
CDL_TEST_CASE(GuiSystem, UploadsImGuisFontAtlasIntoThePool, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		const GuiDrawData gui = RunGuiFrame(*test.Gui, DrawTwoWindows);

		const ImTextureData* atlas = ImGui::GetIO().Fonts->TexData;
		CDL_CHECK(atlas);
		CDL_EXPECT_EQ(atlas->Status, ImTextureStatus_OK);

		const auto atlasCommand = std::ranges::find_if(gui.Commands, [&](const GuiDrawCommand& command) {
			return ToGuiTextureID(command.Texture) == atlas->GetTexID(); });
		CDL_CHECK_MSG(atlasCommand != gui.Commands.end(), "no command draws with the font atlas, so text can't have been drawn");

		const TextureInfo info = test.Pool.GetTextureInfo(atlasCommand->Texture);
		CDL_EXPECT(info.Extent == glm::uvec2(atlas->Width, atlas->Height));
		CDL_EXPECT_FALSE(info.Usage & vk::ImageUsageFlagBits::eColorAttachment);

		for (const GPUTextureHandle texture : DistinctTextures(gui))
			CDL_EXPECT_MSG(test.Pool.IsAlive(texture), std::format("texture {} generation {} isn't alive", texture.Index, texture.Generation));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Destroying the GUI hands its textures back to the pool, which frees them once the frame carrying the release retires.
CDL_TEST_CASE(GuiSystem, ReleasesItsTexturesWhenDestroyed, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		const uint32_t before = LiveAllocations(test.Allocator);	// No texture yet: ImGui asks for its atlas in the first frame
		const std::vector<GPUTextureHandle> textures = DistinctTextures(RunGuiFrame(*test.Gui, DrawTwoWindows));
		CDL_CHECK_FALSE(textures.empty());

		test.Gui.reset();
		for (const GPUTextureHandle texture : textures)
			CDL_EXPECT(!test.Pool.IsAlive(texture));

		test.Device.WaitIdle();	// The atlas's upload, so its staging buffer is free to go too
		RunReleaseFrame(test);
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), before);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// An exception out of OnUpdate unwinds between StartFrame and EndFrame, when ImGui has no draw data to list the textures
// in, so the release can't depend on it.
CDL_TEST_CASE(GuiSystem, ReleasesItsTexturesWhenDestroyedMidFrame, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		const uint32_t before = LiveAllocations(test.Allocator);
		const std::vector<GPUTextureHandle> textures = DistinctTextures(RunGuiFrame(*test.Gui, DrawTwoWindows));
		CDL_CHECK_FALSE(textures.empty());

		test.Gui->StartFrame();
		CDL_CHECK(ImGui::GetDrawData() == nullptr);	// What made the mid-frame case different
		test.Gui.reset();
		for (const GPUTextureHandle texture : textures)
			CDL_EXPECT(!test.Pool.IsAlive(texture));

		test.Device.WaitIdle();
		RunReleaseFrame(test);
		CDL_EXPECT_EQ(LiveAllocations(test.Allocator), before);
	}
	ExpectNoVulkanDebugMessages(logs);
}

// A handle goes into ImGui::Image as an ImTextureID and must come out of the draw data unchanged, its generation
// included. The invalid handle, which RenderContext gives when there is no output target, must come back invalid.
CDL_TEST_CASE(GuiSystem, TextureHandlesRoundTripThroughImGui, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		const GPUTextureHandle handle{ .Index = 0x00C0FFEE, .Generation = 0x0BADF00D };
		const GuiDrawData gui = RunGuiFrame(*test.Gui, [&] {
			BeginTestWindow("Images");
			ImGui::Image(ToGuiTextureID(handle), { 16.0f, 16.0f });
			ImGui::Image(ToGuiTextureID(GPUTextureHandle{}), { 16.0f, 16.0f });
			ImGui::End();
		});

		CDL_EXPECT(std::ranges::any_of(gui.Commands, [&](const GuiDrawCommand& command) { return SameHandle(command.Texture, handle); }));
		CDL_EXPECT(std::ranges::any_of(gui.Commands, [](const GuiDrawCommand& command) { return !command.Texture.IsValid(); }));
	}
	ExpectNoVulkanDebugMessages(logs);
}

//////////////////////////////////////////////////////////////////////////
// Draw data extraction
//////////////////////////////////////////////////////////////////////////

// Every vertex each extracted command draws, reached through the merged arrays, must be the one ImGui's own draw list
// reaches, so a wrong vertex base, index base or offset in any list shows. Also checks the totals and the display rect.
CDL_TEST_CASE(GuiSystem, ExtractedDrawDataDrawsWhatImGuiDrew, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		const GuiDrawData gui = RunGuiFrame(*test.Gui, DrawTwoWindows);
		const ImDrawData* drawData = ImGui::GetDrawData();
		CDL_CHECK(drawData);
		CDL_CHECK_MSG(drawData->CmdLists.Size >= 2, "one draw list leaves the per-list offsets untested");

		CDL_EXPECT_EQ(gui.Vertices.size(), static_cast<size_t>(drawData->TotalVtxCount));
		CDL_EXPECT_EQ(gui.Indices.size(), static_cast<size_t>(drawData->TotalIdxCount));
		CDL_EXPECT(gui.DisplayPos == glm::vec2(drawData->DisplayPos.x, drawData->DisplayPos.y));
		CDL_EXPECT(gui.DisplaySize == glm::vec2(drawData->DisplaySize.x, drawData->DisplaySize.y));

		const auto imguiCommands = DrawableCommands(*drawData);
		CDL_CHECK_EQ(gui.Commands.size(), imguiCommands.size());
		for (size_t c = 0; c < imguiCommands.size(); ++c)
		{
			const auto [list, imguiCommand] = imguiCommands[c];
			const GuiDrawCommand& command = gui.Commands[c];
			CDL_EXPECT_EQ(command.IndexCount, imguiCommand->ElemCount);

			for (uint32_t i = 0; i < imguiCommand->ElemCount; ++i)
			{
				const ImDrawVert& expected = list->VtxBuffer[imguiCommand->VtxOffset + list->IdxBuffer[imguiCommand->IdxOffset + i]];
				const size_t index = size_t{ command.IndexOffset } + i;
				CDL_CHECK_MSG(index < gui.Indices.size(), std::format("command {} reads index {} of {}", c, index, gui.Indices.size()));
				const size_t vertex = size_t{ command.VertexOffset } + gui.Indices[index];
				CDL_CHECK_MSG(vertex < gui.Vertices.size(), std::format("command {} reads vertex {} of {}", c, vertex, gui.Vertices.size()));

				if (std::memcmp(&gui.Vertices[vertex], &expected, sizeof(ImDrawVert)) != 0)
				{
					CDL_EXPECT_MSG(false, std::format("command {}, element {}: vertex {} isn't the one ImGui drew", c, i, vertex));
					break;
				}
			}
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// ImGui's clip rects are in display coordinates and the renderer's scissors in framebuffer pixels, so extraction removes
// DisplayPos and applies FramebufferScale. On one 1x display both are identities, so they're forced to values that aren't.
CDL_TEST_CASE(GuiSystem, ClipRectsAreExtractedInFramebufferPixels, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		test.Gui->StartFrame();
		DrawTwoWindows();
		test.Gui->EndFrame();

		ImDrawData* drawData = ImGui::GetDrawData();
		CDL_CHECK(drawData);
		const ImVec2 pos{ 100.0f, 50.0f };
		const ImVec2 scale{ 2.0f, 1.5f };
		drawData->DisplayPos = pos;
		drawData->FramebufferScale = scale;

		const GuiDrawData gui = test.Gui->ExtractDrawData();
		CDL_EXPECT(gui.DisplayPos == glm::vec2(pos.x, pos.y));

		const auto imguiCommands = DrawableCommands(*drawData);
		CDL_CHECK_EQ(gui.Commands.size(), imguiCommands.size());
		for (size_t c = 0; c < imguiCommands.size(); ++c)
		{
			const ImVec4& clip = imguiCommands[c].second->ClipRect;
			const glm::vec4 expected{ (clip.x - pos.x) * scale.x, (clip.y - pos.y) * scale.y, (clip.z - pos.x) * scale.x, (clip.w - pos.y) * scale.y };
			CDL_EXPECT_MSG(gui.Commands[c].ClipRect == expected, std::format("command {}", c));
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// The renderer has nothing to call a user callback with, so those commands are dropped and the rest still extracted.
CDL_TEST_CASE(GuiSystem, ExtractionSkipsUserCallbacks, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		const GuiDrawData gui = RunGuiFrame(*test.Gui, [] {
			BeginTestWindow("Callbacks");
			ImGui::Text("Before");
			ImGui::GetWindowDrawList()->AddCallback(NoOpCallback, nullptr);
			ImGui::Text("After");
			ImGui::End();
		});

		const ImDrawData* drawData = ImGui::GetDrawData();
		CDL_CHECK(drawData);
		int callbacks = 0;
		for (const ImDrawList* list : drawData->CmdLists)
			callbacks += static_cast<int>(std::ranges::count_if(list->CmdBuffer, [](const ImDrawCmd& command) { return command.UserCallback != nullptr; }));

		CDL_CHECK_EQ(callbacks, 1);
		CDL_EXPECT_EQ(gui.Commands.size(), DrawableCommands(*drawData).size());
		CDL_EXPECT_FALSE(gui.Commands.empty());
	}
	ExpectNoVulkanDebugMessages(logs);
}

//////////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////////

// Application drops an event the GUI wants, so the game never sees a click meant for a window. Which events count follows
// ImGui's two capture flags, which StartFrame recomputes and Application reads between frames; set by hand here to cover
// each pairing.
CDL_TEST_CASE(GuiSystem, WantsInputFollowsImGuisCaptureFlags, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		RunGuiFrame(*test.Gui, DrawTwoWindows);
		ImGuiIO& io = ImGui::GetIO();

		for (const bool mouse : { false, true })
		{
			for (const bool keyboard : { false, true })
			{
				io.WantCaptureMouse = mouse;
				io.WantCaptureKeyboard = keyboard;
				const std::string flags = std::format("WantCaptureMouse {}, WantCaptureKeyboard {}", mouse, keyboard);

				for (const InputEvent& event : kKeyEvents)
					CDL_EXPECT_MSG(test.Gui->WantsInput(event) == keyboard, std::format("{}: key event {}", flags, event.index()));
				for (const InputEvent& event : kMouseEvents)
					CDL_EXPECT_MSG(test.Gui->WantsInput(event) == mouse, std::format("{}: mouse event {}", flags, event.index()));
			}
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}

// GuiSystem hooks SDL's raw events, so input reaches ImGui through the same pump that feeds the engine, which still gets
// the event as well. A wheel event names its window, which must be the one ImGui was set up on.
CDL_TEST_CASE(GuiSystem, PumpedPlatformEventsReachImGui, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		std::vector<PlatformEvent> events;
		Platform::PumpEvents(events);	// The window's own startup events, out of the way

		SDL_Event wheel{};
		wheel.type = SDL_EVENT_MOUSE_WHEEL;
		wheel.wheel.windowID = test.AppWindow.GetID();
		wheel.wheel.y = 1.0f;
		wheel.wheel.integer_y = 1;
		CDL_CHECK_MSG(SDL_PushEvent(&wheel), SDL_GetError());

		events.clear();
		Platform::PumpEvents(events);
		CDL_EXPECT(std::ranges::any_of(events, [](const PlatformEvent& event) {
			const InputEvent* input = std::get_if<InputEvent>(&event);
			return input && std::holds_alternative<MouseWheelEvent>(*input); }));

		// Read off ImGui's queue rather than io.MouseWheel after a NewFrame: ImGui trickles queued input over frames, so a real
		// mouse move the OS queued first, as when the window opens under the cursor, would hold the wheel back a frame.
		const ImVector<ImGuiInputEvent>& queue = ImGui::GetCurrentContext()->InputEventsQueue;
		CDL_EXPECT(std::ranges::any_of(queue, [](const ImGuiInputEvent& event) {
			return event.Type == ImGuiInputEventType_MouseWheel && event.MouseWheel.WheelY == 1.0f; }));
	}
	ExpectNoVulkanDebugMessages(logs);
}

// Disabling navigation, as a game does when it takes the keyboard and gamepad, clears only ImGui's two navigation flags
// and drops the nav cursor and focus, so the last-focused widget doesn't stay lit or keep taking keys; enabling restores
// exactly the flags construction set. Each is idempotent.
CDL_TEST_CASE(GuiSystem, NavigationTurnsOffAndBackOn, Gpu)
{
	LoggerFixture logs(LogLevel::Warn, kRenderChannelMask);
	{
		TestGui test;
		constexpr ImGuiConfigFlags kNavigation = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
		const ImGuiIO& io = ImGui::GetIO();
		const ImGuiContext& context = *ImGui::GetCurrentContext();

		RunGuiFrame(*test.Gui, [] {
			ImGui::SetNextWindowFocus();
			ImGui::Begin("Focused");
			ImGui::Button("Button");
			ImGui::End();
		});
		ImGui::SetNavCursorVisible(true);
		CDL_CHECK(context.NavWindow != nullptr);
		CDL_CHECK(context.NavCursorVisible);

		for (int i = 0; i < 2; ++i)
		{
			test.Gui->SetNavigationEnabled(false);
			CDL_EXPECT_FALSE(test.Gui->IsNavigationEnabled());
			CDL_EXPECT_EQ(io.ConfigFlags & kNavigation, 0);
			CDL_EXPECT(io.ConfigFlags & ImGuiConfigFlags_DockingEnable);
			CDL_EXPECT(context.NavWindow == nullptr);
			CDL_EXPECT_FALSE(context.NavCursorVisible);
		}

		for (int i = 0; i < 2; ++i)
		{
			test.Gui->SetNavigationEnabled(true);
			CDL_EXPECT(test.Gui->IsNavigationEnabled());
			CDL_EXPECT_EQ(io.ConfigFlags & kNavigation, kNavigation);
			CDL_EXPECT(io.ConfigFlags & ImGuiConfigFlags_DockingEnable);
		}
	}
	ExpectNoVulkanDebugMessages(logs);
}
