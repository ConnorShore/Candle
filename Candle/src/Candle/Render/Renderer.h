#pragma once

namespace Candle {

	class RenderDevice;

	// Turns one packet into recorded GPU work. Knows nothing about threads, windows or presentation.
	// Threading: render thread only.
	class Renderer
	{
	public:
		explicit Renderer(RenderDevice& device);        // pipelines, bindless tables, render graph
		//void Render(const FramePacket& packet, FrameContext& frame, const Image& target);       // target is imported into the graph; its owner presents it
	};

}