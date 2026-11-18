#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace Candle {

	struct FrameGraphContext;

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
		FrameGraphPassNode(std::string_view name, FrameGraphPassConcept* pass)
			: Name(name), Pass(pass) {
		}

		std::string Name;
		FrameGraphPassConcept* Pass;	// Owned by the frame arena, valid until Reset
	};

}
