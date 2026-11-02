#pragma once

#include "Candle/Render/Resources/GPUHandles.h"
#include "Candle/Render/GUI/GuiTextureID.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace Candle {

	class RenderManager;

	// What the application controls about rendering, handed to it as UpdateContext::Render.
	// Threading: main thread only.
	class RenderContext
	{
	public:
		RenderContext(RenderManager* renderManager, bool renderToSwapChain, glm::uvec2 initialOutputSize);	// renderManager is null when headless
		~RenderContext();

		RenderContext(const RenderContext&) = delete;
		RenderContext& operator=(const RenderContext&) = delete;

		// Ignored when rendering to the swapchain. A new size replaces the output target
		void SetOutputSize(glm::vec2 size);
		inline glm::uvec2 GetOutputSize() const { return m_OutputSize; }

		// Fetch after SetOutputSize each frame. Invalid when rendering to the swapchain
		inline GPUTextureHandle GetOutputTexture() const { return m_OutputTarget; }
		inline uint64_t GetOutputTextureID() const { return ToGuiTextureID(m_OutputTarget); }	// For ImGui::Image

		inline bool IsRenderingToSwapChain() const { return m_RenderToSwapChain; }

	private:
		RenderManager* m_RenderManager;
		bool m_RenderToSwapChain;
		glm::uvec2 m_OutputSize;
		GPUTextureHandle m_OutputTarget;
	};

}
