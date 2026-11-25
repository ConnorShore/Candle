#pragma once

#include "FrameGraphSnapshot.h"

#include "Candle/Render/Resources/GPUHandles.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace Candle {

	class TexturePool;
	struct FramePacket;

	// The preview samples as float, so integer formats can't be shown, and a depth-stencil view has both aspects, which can't be sampled
	bool IsPreviewableFormat(vk::Format format);

	// What the preview target holds, as of the frame whose snapshot showed the preview pass drawing it
	struct FrameGraphPreview
	{
		GPUTextureHandle Target;
		std::string Texture;
		uint64_t FrameIndex{ 0 };
	};

	// Between the render graph viewer and the render thread. A request rides the next packet, and its snapshot comes back in a
	// later one, a few frames behind, like RenderStats. Owns the preview target the viewer shows and the render thread draws into.
	// Threading: main thread only.
	class FrameGraphCapture
	{
	public:
		FrameGraphCapture(TexturePool& texturePool, vk::Format previewFormat);
		~FrameGraphCapture();

		FrameGraphCapture(const FrameGraphCapture&) = delete;
		FrameGraphCapture& operator=(const FrameGraphCapture&) = delete;

		// For the next packet only, so a viewer that stops asking, closed or hidden, stops costing anything
		void Request(FrameGraphCaptureRequest request);

		const FrameGraphSnapshot* GetSnapshot() const { return m_HasSnapshot ? &m_Snapshot : nullptr; }	// The newest to come back
		const std::optional<FrameGraphPreview>& GetPreview() const { return m_Preview; }	// Empty until the preview pass has drawn the current target

		// Called by RenderManager::BeginFramePacket: takes the snapshot the render thread left in the packet, then writes this frame's request into it
		void ExchangeWithPacket(FramePacket& packet);

	private:
		void UpdatePreviewTarget(const std::string& textureName, uint64_t frameIndex);
		void ReleasePreviewTarget();
		void UpdatePreview();

	private:
		TexturePool& m_TexturePool;
		vk::Format m_PreviewFormat;

		FrameGraphCaptureRequest m_Request;
		FrameGraphSnapshot m_Snapshot;
		bool m_HasSnapshot{ false };

		GPUTextureHandle m_PreviewTarget;
		glm::uvec2 m_PreviewExtent{ 0, 0 };
		uint64_t m_PreviewFirstFrame{ 0 };	// The first packet to carry the current target; snapshots from before it drew an older one
		std::optional<FrameGraphPreview> m_Preview;
	};

}
