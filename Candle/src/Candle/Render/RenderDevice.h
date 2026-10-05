#pragma once


namespace Candle {

	// Holds the vulkan physical & logical device, queues, command and resource pools, frame timeline, deletion queues etc
	// Will own buffer pools to be allocated from, but the buffers themselves will be owned by the Mesh asset
	// this will own command pools as well, but CommandLists will be short lived and handed out each frame

	// Resource creation can happen from any thread, but submission is from render thread only
	class RenderDevice
	{
	public:


	private:

	};

}