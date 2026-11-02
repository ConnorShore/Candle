#include "cdlpch.h"
#include "RenderContext.h"

#include "Candle/Render/RenderManager.h"
#include "Candle/Platform/Platform.h"

namespace Candle {

	RenderContext::RenderContext(RenderManager* renderManager, bool renderToSwapChain, glm::uvec2 initialOutputSize)
		: m_RenderManager(renderManager)
		, m_RenderToSwapChain(renderToSwapChain)
		, m_OutputSize(glm::max(initialOutputSize, glm::uvec2(1)))
	{
		// Created up front, so every packet has a target to render into even before the application picks a size
		if (m_RenderManager && !m_RenderToSwapChain)
			m_OutputTarget = m_RenderManager->CreateOutputTarget(m_OutputSize);
	}

	RenderContext::~RenderContext()
	{
		if (m_OutputTarget.IsValid())
			m_RenderManager->GetTexturePool().ReleaseTexture(m_OutputTarget);
	}

	void RenderContext::SetOutputSize(glm::vec2 size)
	{
		CDL_CORE_ASSERT(Platform::IsMainThread(), "RenderContext::SetOutputSize must be called on the main thread");

		// At least 1x1, since a collapsed panel reports zero or less
		const glm::uvec2 outputSize = glm::uvec2(glm::max(size, glm::vec2(1.0f)));
		if (outputSize == m_OutputSize)
			return;

		m_OutputSize = outputSize;
		if (!m_OutputTarget.IsValid())
			return;

		// Released rather than resized: frames in flight may still sample the old one, and the pool frees it once they retire
		m_RenderManager->GetTexturePool().ReleaseTexture(m_OutputTarget);
		m_OutputTarget = m_RenderManager->CreateOutputTarget(m_OutputSize);
	}

}
