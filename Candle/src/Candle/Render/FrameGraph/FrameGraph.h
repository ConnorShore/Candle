#pragma once

#include "FrameGraphPass.h"
#include "FrameGraphHandles.h"
#include "FrameGraphAccess.h"
#include "FrameGraphBuilder.h"
#include "ResourceCache.h"
#include "FrameGraphCompilerResources.h"
#include "FrameGraphContext.h"
#include "FrameGraphSnapshot.h"
#include "Blackboard.h"

#include "Candle/Render/Resources/GPUHandles.h"
#include "Candle/Render/Vulkan/Image.h"

#include "Candle/Core/Memory/Collections/ArenaAllocator.h"

#include <glm/glm.hpp>

#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Candle {

	class Blackboard;
	class SwapChainImage;
	class BindlessTable;
	class TexturePool;
	class PipelineStore;
	class GPUAllocator;
	class RenderDevice;

	struct FrameGraphSpec
	{
		Candle::GPUAllocator& Allocator;
		Candle::BindlessTable& BindlessTable;
		Candle::TexturePool& TexturePool;
		Candle::PipelineStore& PipelineStore;
	};

	// Empty (default) type for PassData
	struct FGNoData {};

	struct FGTextureEntry
	{
		std::string_view Name;	// Must outlive the frame, e.g. a string literal
		ImageDesc Desc;			// Imports: from the pool or swapchain. Transients: sized by CreateTexture, with Usage gathered at compile
		bool Imported{ false };
		vk::Image Image{ nullptr };
		vk::ImageView ImageView{ nullptr };
		uint32_t BindlessIndex{ std::numeric_limits<uint32_t>::max() };	// None for the swapchain
		vk::ImageLayout SampledLayout{ vk::ImageLayout::eShaderReadOnlyOptimal };
		ImageState InitialState, FinalState;
		vk::PipelineStageFlags2 VisibleStages{};	// Where the last write, or layout change, has been made visible: a read anywhere else needs a barrier
		vk::AccessFlags2 VisibleAccess{};
		uint32_t CacheIndex{ std::numeric_limits<uint32_t>::max() };	// Transients only
		uint32_t LatestVersion{ 0 };	// Bumped by every write, so a handle from before the last write is caught as stale
		uint32_t LastWritePassIndex{ std::numeric_limits<uint32_t>::max() };	// Set by the builder during setup: the last pass so far to write it, or a transient's creator
	};

	class FrameGraph
	{
	public:
		FrameGraph(FrameGraphSpec spec);
		~FrameGraph();

		void Reset(glm::uvec2 outputExtent);
		void Compile(uint64_t frameIndex);
		void EvictUnusedResources(uint64_t frameIndex, uint64_t lastSubmittedValue, uint64_t completedValue);	// After Compile, so this frame's transients count as used
		void Execute(const RenderDevice& renderDevice, vk::raii::CommandBuffer& commandBuffer);

		template<typename PassData = FGNoData, typename SetupFunc, typename ExecuteFunc>
		const PassData& AddRasterPass(std::string_view name, SetupFunc setup, ExecuteFunc execute, bool NeverCull = false)
		{
			return AddPass<PassData>(name, setup, execute, false, NeverCull);
		}

		template<typename PassData = FGNoData, typename SetupFunc, typename ExecuteFunc>
		const PassData& AddComputePass(std::string_view name, SetupFunc setup, ExecuteFunc execute, bool NeverCull = false)
		{
			return AddPass<PassData>(name, setup, execute, true, NeverCull);
		}

		FGTextureHandle ImportSwapChainImage(SwapChainImage& image);
		FGTextureHandle ImportTexture(std::string_view name, GPUTextureHandle texture);

		inline const FGTextureEntry& GetTexture(FGTextureHandle handle) const
		{
			CDL_CORE_ASSERT(handle.Index < m_Textures.size(), "FGTextureHandle is not from this frame");
			return m_Textures[handle.Index];
		}

		// The latest version of the first texture with this name, which any pass added after this point can read
		std::optional<FGTextureHandle> FindTexture(std::string_view name) const;

		// Valid from Compile until the next Reset
		inline const FGCompiledGraph& GetCompiledGraph() const { return m_CompiledGraph; }
		inline Blackboard& GetBlackboard() { return m_Blackboard; }

		// From Compile until the next Reset. Reuses out's storage, so capturing every frame doesn't allocate once it has grown.
		void CaptureSnapshot(FrameGraphSnapshot& out) const;


	private:
		friend class FrameGraphBuilder;
		friend class FrameGraphContext;

		template<typename PassData = FGNoData, typename SetupFunc, typename ExecuteFunc>
		const PassData& AddPass(std::string_view name, SetupFunc setup, ExecuteFunc execute, bool IsCompute = false, bool NeverCull = false)
		{
			CDL_STATIC_ASSERT(std::is_trivially_destructible_v<PassData>, "PassData lives in the frame arena, which never runs destructors");
			CDL_STATIC_ASSERT(std::is_trivially_destructible_v<ExecuteFunc>,
				"Execute's captures live in the frame arena, which never runs destructors; capture handles and pointers, not containers");

			using Pass = FrameGraphPass<PassData, ExecuteFunc>;
			Pass* pass = m_FrameArena.New<Pass>(std::move(execute));
			FrameGraphPassNode node(name, pass, IsCompute, NeverCull);
			node.AccessStart = static_cast<uint32_t>(m_Accesses.size());

			const uint32_t passIndex = static_cast<uint32_t>(m_Passes.size());
			FrameGraphBuilder builder(*this, passIndex);	// TODO: Use concepts to enforce that the setup function takes a FrameGraphBuilder& and PassData& as parameters
			setup(builder, pass->Data);

			// A pass added from inside setup would take this pass's index and split its accesses in two
			CDL_CORE_ASSERT(m_Passes.size() == passIndex, "AddPass can't be called from inside another pass's setup");
			node.AccessCount = static_cast<uint32_t>(m_Accesses.size()) - node.AccessStart;
			m_Passes.push_back(std::move(node));

			// Arena memory never moves, unlike m_Passes, so this stays valid until Reset
			return pass->Data;
		}

		inline std::span<const FGAccessEntry> GetPassAccesses(uint32_t passIndex) const
		{
			const FrameGraphPassNode& pass = m_Passes[passIndex];
			return std::span(m_Accesses).subspan(pass.AccessStart, pass.AccessCount);
		}

		void CullPasses();

	private:
		TexturePool& m_TexturePool;
		PipelineStore& m_PipelineStore;
		ArenaAllocator m_FrameArena;
		ResourceCache m_ResourceCache;
		FGCompiledGraph m_CompiledGraph;
		Blackboard m_Blackboard;

		glm::uvec2 m_OutputExtent{ 0, 0 };
		std::vector<FrameGraphPassNode> m_Passes;

		// The frame graph owns the textures, which are either imported from the pool or swapchain, or created as transients. The passes only have handles to them.
		std::vector<FGTextureEntry> m_Textures;
		//std::vector<FGBufferHandle> m_Buffers;

		std::vector<FGAccessEntry> m_Accesses;	// Every pass's reads and writes, each pass's in one run (see GetPassAccesses)

		bool m_ResetBeforeCompile{ false };
	};

}