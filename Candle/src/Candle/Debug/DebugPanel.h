#pragma once

#include "DebugContext.h"

namespace Candle {

	enum class DebugPanelCategory
	{
		General,
		Rendering,
		Memory,
		Audio,
		Physics,
		Scene
	};

	struct DebugPanelCommand
	{
		std::string Name;
		std::string Description;
		std::string Usage;
	};

	class DebugPanel
	{
	public:
		bool IsOpen = true;

	public:
		DebugPanel() = default;
		virtual ~DebugPanel() = default;

		// Delete copy/move constructors and assignment operators to prevent copying or moving DebugPanel instances
		DebugPanel(const DebugPanel&) = delete;
		DebugPanel(DebugPanel&&) = delete;
		DebugPanel& operator=(const DebugPanel&) = delete;
		DebugPanel& operator=(DebugPanel&&) = delete;

		virtual void OnUpdate(const DebugContext& context) { }		// Called every frame, even if not visible
		virtual void OnRender(const DebugContext& context) = 0;		// Called every frame when visible

		virtual const char* GetName() const = 0;
		virtual DebugPanelCategory GetCategory() const = 0;
		virtual DebugPanelCommand GetCommand() const = 0;	// Single command to show/hide panel for now; TODO: may add ability to add more commands later
	};

}