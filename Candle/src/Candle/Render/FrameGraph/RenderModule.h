#pragma once

#include "Candle/Render/FramePacket.h"
#include "Candle/Render/FrameContext.h"
#include "Candle/Render/ShaderInterop/GlobalPushConstants.h"
#include "Candle/Core/Memory/SharedPtr.h"

#include <filesystem>
#include <string>

namespace Candle {

	class FrameGraph;
	class PipelineStore;
	class Shader;

	// Panics on failure, since the renderer can't run without its own shaders.
	// TODO: Temporary until the asset manager loads shaders
	SharedPtr<Shader> LoadEngineShader(const std::string& name, const std::filesystem::path& filePath);

	struct RenderFrameInputs
	{
		const FramePacket& Packet;
		FrameContext& Frame;
		const GPU::GlobalPushConstants& Global;
	};

	struct RenderModuleSpec
	{
		FrameGraph& Graph;
		PipelineStore& Pipelines;
		vk::Format ColorFormat;
		vk::Format DepthFormat;
		std::filesystem::path ShaderDirectory;
	};

	class RenderModule
	{
	public:
		virtual ~RenderModule() = default;

		virtual void AddPasses(const RenderFrameInputs& inputs) = 0;
		virtual const char* GetName() const = 0;
	};

}