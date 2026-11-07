#pragma once

#include "DebugDrawData.h"
#include "DebugDraw.h"

#include "Candle/Core/Time.h"
#include "Candle/Core/Memory/Collections/MPSCRingBuffer.h"

#include <glm/glm.hpp>

#include <vector>
#include <array>
#include <cstdint>
#include <variant>

namespace Candle {

	constexpr size_t kDebugDrawEntryCount = 1024 * 1024;	// ~1M entries


	// TODO: Add width to the drawing to make connecting lines thicker
	struct DebugDrawPoint
	{
		glm::vec3 Position;
		float Size;
	};

	struct DebugDrawLine
	{
		glm::vec3 Start, End;
	};

	struct DebugDrawCube
	{
		glm::vec3 Position;
		glm::vec3 Scale;
	};

	struct DebugDrawSphere
	{
		glm::vec3 Position;
		float Radius;
	};

	using DebugDrawPrimitive = std::variant<DebugDrawPoint, DebugDrawLine, DebugDrawCube, DebugDrawSphere>;

	// Manages debug draw primitives and their rendering
	// Threading: Can submit debug draw commands from any thread
	//  Extraction happens on main thread, and passes data to the render thread through the FramePacket
	class DebugDrawManager
	{
	public:
		struct DebugDrawEntry
		{
			DebugDrawPrimitive Primitive;
			DebugDrawBucket Bucket;
			uint16_t Color;
			float Duration{ 0.0f };
		};

	public:
		DebugDrawManager();
		~DebugDrawManager();

		void Submit(DebugDrawEntry entry);
		void ExtractDebugDrawData(DebugDrawData& outData, TimeStep delta);

	private:
		MPSCRingBuffer<DebugDrawEntry> m_DrawQueue{ kDebugDrawEntryCount };	// 1M entries, ~64MB
		std::vector<DebugDrawEntry> m_Persistent;
	};
}