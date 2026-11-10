#pragma once

#include "PipelineSpecification.h"
#include "Candle/Render/Resources/GPUHandles.h"

#include <vulkan/vulkan_raii.hpp>

#include <expected>
#include <vector>

namespace Candle {

	// The one push-constant range every pipeline layout declares, so pushed values survive pipeline binds.
	// All stages (compute included, once it shares layouts); 128 bytes is the maxPushConstantsSize every device guarantees.
	inline constexpr vk::ShaderStageFlags kPushConstantStages = vk::ShaderStageFlagBits::eAll;
	inline constexpr uint32_t kPushConstantsSize = 128;

	class RenderDevice;
	class BindlessTable;

	// Threading: not synchronized. CreatePipeline runs on the main thread before the render thread starts, and the render
	// thread only reads from then on (Thread::Start is the hand-off). Creating a pipeline after that races with those reads.
	class PipelineStore
	{
	public:
		PipelineStore(RenderDevice& renderDevice, BindlessTable& bindlessTable, const std::filesystem::path& pipelineCacheDirectory);
		~PipelineStore();

		GPUPipelineHandle CreatePipeline(PipelineSpecification spec);

		// Returns nullptr for an invalid or not ready handle (i.e hasn't fully built yet)
		// TODO: Probably need some async creation for this to be useful, but for now we just block until the pipeline is ready
		vk::Pipeline TryGetPipeline(GPUPipelineHandle handle);
		std::expected<ShaderEntryPoint, std::string> GetEntryPoint(GPUPipelineHandle handle, ShaderStage stage) const;

		inline vk::PipelineLayout GetGlobalLayout() const { return m_GlobalLayout; }

	private:
		inline static constexpr std::string_view kPipelineCacheFileName = "PipelineCache.bin";
		struct PipelineEntry
		{
			vk::raii::Pipeline Pipeline{ nullptr };
			PipelineSpecification Spec;
			//std::array<ShaderEntryPoint, 2> EntryPoints;	// Max of 2: (vert, frag) but can be 1 (vert, {}) or (compute, {})
			// TODO: May need to keep entry points here too but will see
		};

		vk::raii::PipelineLayout CreateGlobalLayout();
		std::expected<PipelineEntry, std::string> CreateGraphicsPipelineEntry(const GraphicsPipelineSpecification& spec);
		std::expected<PipelineEntry, std::string> CreateComputePipelineEntry(const ComputePipelineSpecification& spec);

		std::expected<void, std::string> ValidateHandle(GPUPipelineHandle handle) const;

		void InitializePipelineCache();

	private:
		RenderDevice& m_RenderDevice;
		BindlessTable& m_BindlessTable;

		// Every pipeline layout is compatible with this one, so the render thread can bind any pipeline after binding this layout
		vk::raii::PipelineLayout m_GlobalLayout{ nullptr };

		// TODO: May need to make thread safe in future if have async pipeline creation,
		// but for now we just block until the pipeline is ready
		std::vector<PipelineEntry> m_Pipelines;

		// Pipeline cache
		vk::raii::PipelineCache m_PipelineCache{ nullptr };

		std::filesystem::path m_PipelineCacheFile;
	};

}