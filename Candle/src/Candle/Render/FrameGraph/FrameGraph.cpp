#include "cdlpch.h"
#include "FrameGraph.h"

#include "Candle/Render/Vulkan/GPUAllocator.h"
#include "Candle/Render/Vulkan/BindlessTable.h"
#include "Candle/Render/Resources/TexturePool.h"

namespace Candle {

	namespace {
		constexpr size_t kFrameArenaSize = 1024 * 1024;	// 1 MB for now, may need to be larger if passes have a lot of data
	}


	FrameGraph::FrameGraph(FrameGraphSpec spec)
		: m_TexturePool(spec.TexturePool)
		, m_FrameArena(kFrameArenaSize)
		, m_ResourceCache(spec.Allocator, spec.BindlessTable)
	{
	}

	FrameGraph::~FrameGraph()
	{

	}

	void FrameGraph::Reset(glm::uvec2 outputExtent)
	{

	}

	void FrameGraph::Compile()
	{

	}

	void FrameGraph::Execute(vk::CommandBuffer& commandBuffer)
	{

	}

	FGTextureHandle FrameGraph::ImportSwapChainImage(SwapChainImage& image, uint64_t timelineValue)
	{
		TextureInfo info = {
			
		}
	}

	FGTextureHandle FrameGraph::ImportTexture(GPUTextureHandle texture, uint64_t timelineValue)
	{
		TextureInfo info = m_TexturePool.GetTextureInfo(texture);
		const GPUImage* image = m_TexturePool.GetTextureGPUImage(texture);

		CacheImportResult result = m_ResourceCache.ImportExistingTexture(info, image, timelineValue);

		FGSize size{ .Mode = FGSizeMode::Absolute, .Extent = info.Desc.Extent };
		FrameGraph::FGTextureEntry entry{
			.Name = "Imported Texture Test",
			.Spec = FGTextureSpec{ .Size = size, .Format = info.Desc.Format },
			.Imported = true,
			.Image = image->GetImage(),
			.ImageView = image->GetImageView(),
			.InitialState = {						// TODO: Need to get access and stage flags
				.Layout = info.SampledLayout, 
				.Access = vk::AccessFlagBits2::eShaderSampledRead, 
				.Stage = vk::PipelineStageFlagBits2::eFragmentShader 
			},
			.FinalState = {
				.Layout = info.SampledLayout,
				.Access = vk::AccessFlagBits2::eShaderSampledRead,
				.Stage = vk::PipelineStageFlagBits2::eFragmentShader
			},
			.CacheIndex = result.Index
		};

		m_Textures.push_back(std::move(entry));
		return FGTextureHandle{ static_cast<uint32_t>(m_Textures.size() - 1) };
	}

}