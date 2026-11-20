#pragma once

#include "FrameGraphPass.h"
#include "FrameGraphHandles.h"
#include "FrameGraphBuilder.h"
#include "ResourceCache.h"
#include "FrameGraphCompilerResources.h"
#include "FrameGraphContext.h"

#include "Candle/Render/Resources/GPUHandles.h"
#include "Candle/Render/Vulkan/Image.h"

#include "Candle/Core/Memory/Collections/ArenaAllocator.h"

#include <glm/glm.hpp>

#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Candle {

	class Blackboard;
	class SwapChainImage;
	class BindlessTable;
	class TexturePool;
	class GPUAllocator;
	class RenderDevice;

	struct FrameGraphSpec
	{
		Candle::GPUAllocator& Allocator;
		Candle::BindlessTable& BindlessTable;
		Candle::TexturePool& TexturePool;
	};

	// Empty (default) type for PassData
	struct FGNoData {};

	struct FGTextureEntry
	{
		std::string_view Name;	// Must outlive the frame, e.g. a string literal
		ImageDesc Desc;			// Imports: from the pool or swapchain. Transients: resolved from Spec at compile
		FGTextureSpec Spec;		// Transients only
		bool Imported{ false };
		vk::Image Image{ nullptr };
		vk::ImageView ImageView{ nullptr };
		uint32_t BindlessIndex{ std::numeric_limits<uint32_t>::max() };	// None for the swapchain
		ImageState InitialState, FinalState;
		uint32_t CacheIndex{ std::numeric_limits<uint32_t>::max() };	// Transients only
	};

	struct FGTextureAccessEntry
	{
		FGTextureHandle Handle;
		FGUsage Usage;
		bool Write{ false };				// If true, the pass writes to the texture, so it needs a layout transition and a barrier
		FGLoad LoadOp{ FGLoad::DontCare };	// Attachments only
		vk::ClearValue ClearVlaue;			// Attachments only
	};

	class FrameGraph
	{
	public:
		FrameGraph(FrameGraphSpec spec);
		~FrameGraph();

		void Reset(glm::uvec2 outputExtent);
		void Compile(uint64_t currentTimelineVal);
		void Execute(const RenderDevice& renderDevice, vk::raii::CommandBuffer& commandBuffer);

		template<typename PassData = FGNoData, typename SetupFunc, typename ExecuteFunc>
		const PassData& AddPass(std::string_view name, SetupFunc setup, ExecuteFunc execute)
		{
			CDL_STATIC_ASSERT(std::is_trivially_destructible_v<PassData>, "PassData lives in the frame arena, which never runs destructors");
			CDL_STATIC_ASSERT(std::is_trivially_destructible_v<ExecuteFunc>,
				"Execute's captures live in the frame arena, which never runs destructors; capture handles and pointers, not containers");

			using Pass = FrameGraphPass<PassData, ExecuteFunc>;
			Pass* pass = m_FrameArena.New<Pass>(std::move(execute));
			FrameGraphPassNode node(name, pass);

			// Add an empty texture access list for this pass, which will be filled by the builder. The builder keeps a reference to it, so it must stay valid until Reset.
			m_TextureAccesses.push_back({});

			FrameGraphBuilder builder(*this, static_cast<uint32_t>(m_Passes.size()));	// TODO: Use concepts to enforce that the setup function takes a FrameGraphBuilder& and PassData& as parameters
			setup(builder, pass->Data);

			m_Passes.push_back(std::move(node));

			// Arena memory never moves, unlike m_Passes, so this stays valid until Reset
			return pass->Data;
		}

		FGTextureHandle ImportSwapChainImage(SwapChainImage& image);
		FGTextureHandle ImportTexture(std::string_view name, GPUTextureHandle texture);

		inline const FGTextureEntry& GetTexture(FGTextureHandle handle) const
		{
			CDL_CORE_ASSERT(handle.Index < m_Textures.size(), "FGTextureHandle is not from this frame");
			return m_Textures[handle.Index];
		}

		// Valid from Compile until the next Reset
		inline const FGCompiledGraph& GetCompiledGraph() const { return m_CompiledGraph; }

	private:
		friend class FrameGraphBuilder;
		friend class FrameGraphContext;

	private:
		TexturePool& m_TexturePool;
		ArenaAllocator m_FrameArena;
		ResourceCache m_ResourceCache;
		FGCompiledGraph m_CompiledGraph;

		glm::uvec2 m_OutputExtent{ 0, 0 };
		std::vector<FrameGraphPassNode> m_Passes;

		// The frame graph owns the textures, which are either imported from the pool or swapchain, or created as transients. The passes only have handles to them.
		std::vector<FGTextureEntry> m_Textures;

		// TODO: Once FG is working, optimize this nested vector (probably to single vector)
		std::vector<std::vector<FGTextureAccessEntry>> m_TextureAccesses;	// Indexed by pass, so each pass has a list of the textures it reads and writes
		//std::vector<FGBufferHandle> m_Buffers;

		bool m_ResetBeforeCompile{ false };
	};

}