#pragma once

#include "FrameGraphContext.h"

#include <string>
#include <string_view>
#include <utility>

namespace Candle {

	struct FrameGraphPassConcept
	{
		virtual void operator()(FrameGraphContext& context) const = 0;

	protected:
		~FrameGraphPassConcept() = default;	// Passes live in the frame arena, so nothing may delete one through its base
	};

	template<typename PassData, typename ExecuteFunc>
	struct FrameGraphPass final : public FrameGraphPassConcept
	{
		FrameGraphPass(ExecuteFunc execute) : Execute(std::move(execute)) {}
		void operator()(FrameGraphContext& context) const override
		{
			Execute(Data, context);
		}

		PassData Data;
		ExecuteFunc Execute;
	};

	struct FrameGraphPassNode
	{
		FrameGraphPassNode(std::string_view name, FrameGraphPassConcept* pass, bool IsCompute, bool NeverCull)
			: Name(name), Pass(pass), IsComputePass(IsCompute), NeverCull(NeverCull) 
		{
		}

		std::string Name;
		FrameGraphPassConcept* Pass;	// Owned by the frame arena, valid until Reset
		bool IsComputePass{ false };	// TODO: Eventually remove this and use the pipeline type to determine if it's a compute pass
		bool NeverCull{ false };
		bool IsLive{ false };	// Set to false if the pass is culled during compilation, so it won't be executed
		uint32_t AccessStart{ 0 }, AccessCount{ 0 };	// Into the graph's access list. Contiguous, since a pass's setup finishes before the next pass is added.
	};

}
