#pragma once

#include "FrameGraphPass.h"
#include "FrameGraphHandles.h"
#include "FrameGraphBuilder.h"
#include "ResourceCache.h"

#include "Candle/Render/Resources/GPUHandles.h"

#include "Candle/Core/Memory/Collections/ArenaAllocator.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>
#include <functional>

namespace Candle {

	class Blackboard;
	class SwapChainImage;
	class BindlessTable;
	class TexturePool;
	class GPUAllocator;

	// Holds things needed for a render pass
	struct FrameGraphContext 
	{
		ResourceCache& Resources;
		Blackboard& Blackboard;
	};

	struct FrameGraphSpec
	{
		Candle::GPUAllocator& Allocator;
		Candle::BindlessTable& BindlessTable;
		Candle::TexturePool& TexturePool;
	};

	// Empty (default) type for PassData
	struct FGNoData {};

	class FrameGraph
	{
	public:
		FrameGraph(FrameGraphSpec spec);
		~FrameGraph();

		void Reset(glm::uvec2 outputExtent);
		void Compile();
		void Execute(vk::CommandBuffer& commandBuffer);

		//TODO: The pass data and lambda need to be added in the ArenaAllocator
		template<typename PassData = FGNoData, typename SetupFunc, typename ExecuteFunc>
		const PassData& AddPass(std::string_view name, SetupFunc setup, ExecuteFunc execute)
		{
			// The pass is allocated in the frame arena, so it will be destroyed when the frame graph is reset
			FrameGraphPassConcept* pass = m_FrameArena.New<FrameGraphPass<PassData, ExecuteFunc>>(std::move(execute));
			FrameGraphPassNode node(name, pass);

			FrameGraphBuilder builder(*this, node);	// TODO: Use concepts to enforce that the setup function takes a FrameGraphBuilder& and PassData& as parameters

			// Call setup then add the node
			std::invoke(setup, builder, PassData);

			m_Passes.push_back(std::move(node));

			return m_Passes.back().Data;
		}

		FGTextureHandle ImportSwapChainImage(SwapChainImage& image, uint64_t timelineValue);
		FGTextureHandle ImportTexture(GPUTextureHandle texture, uint64_t timelineValue);

	private:
		struct FGTextureEntry
		{
			std::string_view Name;
			FGTextureSpec Spec;
			bool Imported{ false };
			vk::Image Image;
			vk::ImageView ImageView;
			ImageState InitialState, FinalState;
			uint32_t CacheIndex;
		};

	private:
		TexturePool& m_TexturePool;
		ArenaAllocator m_FrameArena;
		ResourceCache m_ResourceCache;

		std::vector<FrameGraphPassNode> m_Passes;

		// TODO: Will probably move to ResourceCache
		std::vector<FGTextureEntry> m_Textures;
		//std::vector<FGBufferHandle> m_Buffers;
	};

}