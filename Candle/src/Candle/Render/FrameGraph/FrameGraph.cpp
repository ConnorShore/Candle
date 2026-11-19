#include "cdlpch.h"
#include "FrameGraph.h"

#include "Candle/Render/Vulkan/GPUAllocator.h"
#include "Candle/Render/Vulkan/BindlessTable.h"
#include "Candle/Render/Vulkan/SwapChainImage.h"
#include "Candle/Render/Resources/TexturePool.h"

namespace Candle {

	namespace {
		constexpr size_t kFrameArenaSize = 1024 * 1024;	// 1 MB for now, may need to be larger if passes have a lot of data

		// Every stage a pool texture may be sampled in outside the graph, the same ones GPUUploader publishes uploads to
		constexpr vk::PipelineStageFlags2 kSampledStages = vk::PipelineStageFlagBits2::eVertexShader
			| vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader;

		// Presentation reads it outside our command stream, so the present semaphore does the waiting, as in OldRender
		constexpr ImageState kPresentState{ .Layout = vk::ImageLayout::ePresentSrcKHR, .Access = vk::AccessFlagBits2::eNone };
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
		m_OutputExtent = outputExtent;
		m_Passes.clear();
		m_Textures.clear();
		m_FrameArena.Reset();
		m_TextureAccesses.clear();
	}

	void FrameGraph::Compile()
	{

	}

	void FrameGraph::Execute(vk::CommandBuffer& commandBuffer)
	{

	}

	FGTextureHandle FrameGraph::ImportSwapChainImage(SwapChainImage& image)
	{
		m_Textures.push_back({
			.Name = "SwapChain",
			.Desc = image.GetDesc(),
			.Imported = true,
			.Image = image.GetImage(),
			.ImageView = image.GetImageView(),
			.InitialState = image.GetState(),	// Acquire leaves it at the semaphore's wait stage, so the first barrier chains with the wait
			.FinalState = kPresentState
		});
		return { static_cast<uint32_t>(m_Textures.size() - 1) };
	}

	FGTextureHandle FrameGraph::ImportTexture(std::string_view name, GPUTextureHandle texture)
	{
		// The default handle resolves to the error texture, which no pass may write
		CDL_CORE_ASSERT(texture.IsValid(), "ImportTexture was given an invalid texture handle");
		const TextureInfo info = m_TexturePool.GetTextureInfo(texture);

		m_Textures.push_back({
			.Name = name,
			.Desc = info.Desc,
			.Imported = true,
			.Image = info.Image,
			.ImageView = info.ImageView,
			.BindlessIndex = info.Index,
			// Earlier reads only have to finish before a write, so no access; later reads have to see the writes
			.InitialState = { .Layout = info.SampledLayout, .Access = vk::AccessFlagBits2::eNone, .Stage = kSampledStages },
			.FinalState = { .Layout = info.SampledLayout, .Access = vk::AccessFlagBits2::eShaderSampledRead, .Stage = kSampledStages }
		});
		return { static_cast<uint32_t>(m_Textures.size() - 1) };
	}

	FGTextureHandle FrameGraph::CreateTexture(std::string_view name, FGTextureSpec spec)
	{
		ImageState initialState{
			.Layout = vk::ImageLayout::eUndefined,
			.Access = vk::AccessFlagBits2::eNone,
			.Stage = vk::PipelineStageFlagBits2::eTopOfPipe
		};

		// During compilation, we will walk the textures (non imported) to create resource cache
		// and bindless table entries for them
		
		m_Textures.emplace_back(FGTextureEntry{
			.Name = name,
			.Desc = {
				.Extent = spec.Size.Extent,
				.Format = spec.Format,
				.Usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
				.MipLevels = 1
			},
			.Imported = false,
			.InitialState = initialState,
			.FinalState = initialState	// Will be updated during compilation to the last pass's final state
		});
		return { static_cast<uint32_t>(m_Textures.size() - 1) };
	}

}
