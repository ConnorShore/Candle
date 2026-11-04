#pragma once

#include "Candle/Debug/DebugPanel.h"

namespace Candle {

	class GPUResourceInspectorPanel : public DebugPanel
	{
	public:
		GPUResourceInspectorPanel();
		~GPUResourceInspectorPanel() override;

		virtual void OnUpdate(const DebugContext& context) override;
		virtual void OnRender(const DebugContext& context) override;

		inline virtual const char* GetName() const override { return "GPU Resource Inspector Panel"; }
		inline virtual DebugPanelCategory GetCategory() const override { return DebugPanelCategory::Memory; }
		inline virtual DebugPanelCommand GetCommand() override
		{
			return DebugPanelCommand{
				.Name{ "GPUResources" },
				.Description{ "GPU Resource Inspector Panel" },
				.Usage{ "GPUResources" }
			};
		}

	private:
		void RenderHeapStats(const char* heapName, const GPUHeapStats& stats);
		void RenderPoolStats(const char* poolName, const GPUResourcePoolStats& stats);
	};

}