#include "cdlpch.h"
#include "FrameGraphCapture.h"

#include "Candle/Render/FramePacket.h"
#include "Candle/Render/Resources/TexturePool.h"

#include <vulkan/vulkan_format_traits.hpp>

#include <algorithm>
#include <string_view>
#include <utility>

namespace Candle {

	bool IsPreviewableFormat(vk::Format format)
	{
		const std::string_view numericFormat = vk::componentNumericFormat(format, 0);
		return numericFormat != "UINT" && numericFormat != "SINT" && !vk::hasStencilComponent(format);
	}

	namespace {

		// The texture the preview pass read, if the frame drew the preview at all
		const FrameGraphSnapshot::Texture* FindPreviewedTexture(const FrameGraphSnapshot& snapshot)
		{
			const auto pass = std::ranges::find_if(snapshot.Passes, [](const FrameGraphSnapshot::Pass& pass) { return pass.Name == kFrameGraphPreviewPassName; });
			if (pass == snapshot.Passes.end() || !pass->Live)
				return nullptr;

			for (uint32_t i = 0; i < pass->AccessCount; ++i)
			{
				const FGAccessEntry& access = snapshot.Accesses[pass->AccessStart + i];
				if (!access.Write)
					return &snapshot.Textures[access.ResourceIndex];
			}
			return nullptr;
		}

		const FrameGraphSnapshot::Texture* FindTransient(const FrameGraphSnapshot& snapshot, std::string_view name)
		{
			const auto it = std::ranges::find_if(snapshot.Textures, [name](const FrameGraphSnapshot::Texture& texture) { return !texture.Imported && texture.Name == name; });
			return it != snapshot.Textures.end() ? &*it : nullptr;
		}

	}

	FrameGraphCapture::FrameGraphCapture(TexturePool& texturePool, vk::Format previewFormat)
		: m_TexturePool(texturePool), m_PreviewFormat(previewFormat)
	{
	}

	FrameGraphCapture::~FrameGraphCapture()
	{
		ReleasePreviewTarget();
	}

	void FrameGraphCapture::Request(FrameGraphCaptureRequest request)
	{
		m_Request = std::move(request);
	}

	void FrameGraphCapture::ExchangeWithPacket(FramePacket& packet)
	{
		if (packet.FrameGraphSnapshot.Captured)
		{
			m_Snapshot = packet.FrameGraphSnapshot;
			m_HasSnapshot = true;
			UpdatePreview();
		}

		FrameGraphCaptureRequest request = std::exchange(m_Request, {});
		UpdatePreviewTarget(request.PreviewTexture, packet.FrameIndex);

		// The snapshot is the only way to learn the preview landed
		request.Snapshot = request.Snapshot || request.CapturePreview;
		request.PreviewTarget = request.CapturePreview ? m_PreviewTarget : GPUTextureHandle{};
		packet.FrameGraphCapture = std::move(request);
	}

	void FrameGraphCapture::UpdatePreview()
	{
		if (!m_PreviewTarget.IsValid() || m_Snapshot.FrameIndex < m_PreviewFirstFrame)
			return;

		if (const FrameGraphSnapshot::Texture* drawn = FindPreviewedTexture(m_Snapshot))
			m_Preview = FrameGraphPreview{ .Target = m_PreviewTarget, .Texture = drawn->Name, .FrameIndex = m_Snapshot.FrameIndex };
	}

	void FrameGraphCapture::UpdatePreviewTarget(const std::string& textureName, uint64_t frameIndex)
	{
		// Sized from the newest snapshot; the preview pass scales to its target, so a frame or two at an old size only blurs
		const FrameGraphSnapshot::Texture* texture = m_HasSnapshot && !textureName.empty() ? FindTransient(m_Snapshot, textureName) : nullptr;
		if (!texture || texture->Extent.x == 0 || texture->Extent.y == 0)
		{
			ReleasePreviewTarget();
			return;
		}

		if (m_PreviewTarget.IsValid() && m_PreviewExtent == texture->Extent)
			return;

		// Released rather than resized: frames in flight may still draw or show the old one, and the pool frees it once they retire
		ReleasePreviewTarget();
		m_PreviewTarget = m_TexturePool.CreateRenderTarget({ .Extent = texture->Extent, .Format = m_PreviewFormat, .DebugName = "FrameGraph Preview" });
		m_PreviewExtent = texture->Extent;
		m_PreviewFirstFrame = frameIndex;
	}

	void FrameGraphCapture::ReleasePreviewTarget()
	{
		if (m_PreviewTarget.IsValid())
			m_TexturePool.ReleaseTexture(m_PreviewTarget);

		m_PreviewTarget = {};
		m_PreviewExtent = { 0, 0 };
		m_Preview.reset();
	}

}
