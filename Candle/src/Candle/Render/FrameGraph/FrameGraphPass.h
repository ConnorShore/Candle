#pragma once

#include "Candle/Core/Memory/ScopedPtr.h"

#include <string_view>
#include <vector>
#include <functional>

namespace Candle {

	struct FramePassContext;
	struct FrameGraphResourceHandle;

	struct FrameGraphPassConcept
	{
		FrameGraphPassConcept() = default;
		~FrameGraphPassConcept() = default;

		virtual void operator()(FramePassContext& context) = 0;
	};

	template<typename PassData, typename ExecuteFunc>
	struct FrameGraphPass : public FrameGraphPassConcept
	{
		FrameGraphPass(ExecuteFunc execute) : Execute(std::move(execute)) {}
		void operator()(FramePassContext& context) override
		{
			Execute(context, Data);
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
		FrameGraphPassConcept* Pass;
		std::vector<FrameGraphResourceHandle> ReadResources;
		std::vector<FrameGraphResourceHandle> WriteResources;
	};

}