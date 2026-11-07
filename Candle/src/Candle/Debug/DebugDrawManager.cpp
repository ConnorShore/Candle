#include "cdlpch.h"
#include "DebugDrawManager.h"

namespace Candle {

	namespace {

		static DebugDrawManager* s_Manager{ nullptr };

		constexpr float kDefaultPointSize = 0.5f;

		// Cube corner pairs for a line list: bottom ring, top ring, then the four verticals
		constexpr std::array<uint8_t, 24> kCubeEdgeIndices = {
			0, 1,  1, 2,  2, 3,  3, 0,
			4, 5,  5, 6,  6, 7,  7, 4,
			0, 4,  1, 5,  2, 6,  3, 7
		};

		uint16_t ToColorUint16(const glm::vec4& v) 
		{
			float r = std::clamp(v.x, 0.0f, 1.0f);
			float g = std::clamp(v.y, 0.0f, 1.0f);
			float b = std::clamp(v.z, 0.0f, 1.0f);
			float a = std::clamp(v.w, 0.0f, 1.0f);

			uint16_t r4 = static_cast<uint16_t>(std::round(r * 15.0f));
			uint16_t g4 = static_cast<uint16_t>(std::round(g * 15.0f));
			uint16_t b4 = static_cast<uint16_t>(std::round(b * 15.0f));
			uint16_t a4 = static_cast<uint16_t>(std::round(a * 15.0f));

			return (r4 << 12) | (g4 << 8) | (b4 << 4) | a4;
		}

		void SubmitEntry(DebugDrawPrimitive primitive, glm::vec4 color, const DebugDraw::Options& options)
		{
			if (!s_Manager)
				return;

			DebugDrawBucket bucket = options.DepthTest ? DebugDrawBucket::WorldDepthTested : DebugDrawBucket::WorldNoDepth;
			uint16_t uColor = ToColorUint16(color);
			s_Manager->Submit({
				.Primitive{ primitive },
				.Bucket{ bucket },
				.Color{ uColor },
				.Duration{ options.Duration }
			});
		}
	}

	// DebugDraw implementations
#ifdef CDL_DEBUG_DRAW_ENABLED
	namespace DebugDraw {

		void Cube(glm::vec3 center, glm::vec3 size, glm::vec4 color, const Options& options)
		{
			DebugDrawCube cube{
				.Position{ center },
				.Scale{ size }
			};
			SubmitEntry(cube, color, options);
		}

	}
#endif

	DebugDrawManager::DebugDrawManager()
	{
		CDL_CORE_ASSERT(!s_Manager, "DebugDrawManager already exists!");
		s_Manager = this;
	}

	DebugDrawManager::~DebugDrawManager()
	{
		s_Manager = nullptr;
	}

	//void DebugDrawManager::DrawPoint(glm::vec3 position, glm::vec4 color, float duration /*= 0.0f*/, bool depthEnabled /*= true*/)
	//{
	//	uint16_t uColor = ToColorUint16(color);
	//	DebugDrawPoint point{
	//		.Position{ position },
	//		.Size{ kDefaultPointSize }
	//	};

	//	DebugDrawBucket bucket = depthEnabled ? DebugDrawBucket::WorldDepthTested : DebugDrawBucket::WorldNoDepth;
	//	DebugDrawEntry entry{
	//		.Primitive{ point },
	//		.Bucket{ bucket },
	//		.Color{ uColor },
	//		.Duration{ duration }
	//	};
	//	m_DrawQueue.TryPush(entry);
	//}

	//void DebugDrawManager::DrawCube(glm::vec3 position, glm::vec3 size, glm::vec4 color, float duration /*= 0.0f*/, bool depthEnabled /*= true*/)
	//{
	//	uint16_t uColor = ToColorUint16(color);
	//	DebugDrawCube cube{
	//		.Position{ position },
	//		.Scale{ size }
	//	};

	//	DebugDrawBucket bucket = depthEnabled ? DebugDrawBucket::WorldDepthTested : DebugDrawBucket::WorldNoDepth;
	//	DebugDrawEntry entry{
	//		.Primitive{ cube },
	//		.Bucket{ bucket },
	//		.Color{ uColor },
	//		.Duration{ duration }
	//	};
	//	m_DrawQueue.TryPush(entry);
	//}

	//void DebugDrawManager::DrawSphere(glm::vec3 position, float radius, glm::vec4 color, float duration /*= 0.0f*/, bool depthEnabled /*= true*/)
	//{
	//	uint16_t uColor = ToColorUint16(color);
	//	DebugDrawSphere sphere{
	//		.Position{ position },
	//		.Radius{ radius }
	//	};

	//	DebugDrawBucket bucket = depthEnabled ? DebugDrawBucket::WorldDepthTested : DebugDrawBucket::WorldNoDepth;
	//	DebugDrawEntry entry{
	//		.Primitive{ sphere },
	//		.Bucket{ bucket },
	//		.Color{ uColor },
	//		.Duration{ duration }
	//	};
	//	m_DrawQueue.TryPush(entry);
	//}

	void DebugDrawManager::Submit(DebugDrawEntry entry)
	{
		m_DrawQueue.TryPush(entry);
	}

	void DebugDrawManager::ExtractDebugDrawData(DebugDrawData& outData, TimeStep delta)
	{
		// Clear buckets
		for (auto& bucket : outData.Buckets)
			bucket.clear();

		// TODO: Handle persistent entities.
		//	Loop over persistent items and add to buckets if not yet expired (need to decrement their duration by delta time).
		//  If expired, then swap-remove, otherwise, add to bucket

		// Extract entries from buffer, tessellate and put vertices into proper bucket
		while (auto entry = m_DrawQueue.Pop())
		{
			// If no more entries, break
			if (!entry.has_value())
				break;

			entry = entry.value();

			// TEMPORARY: Only tessellate cubes, ignore other types till cube works correctly
			// TODO: May include indices in the future for better performance as well
			auto& targetBucketVec = outData.Buckets[static_cast<size_t>(entry->Bucket)];
			std::visit(Overloaded{
				[&](const DebugDrawPoint point) {},
				[&](const DebugDrawLine line) {},
				[&](const DebugDrawCube cube) 
				{
					// Line list, so each of the 12 edges is its own pair of vertices
					const glm::vec3 h = cube.Scale * 0.5f;
					const std::array<glm::vec3, 8> corners = {
						cube.Position + glm::vec3(-h.x, -h.y, -h.z),
						cube.Position + glm::vec3( h.x, -h.y, -h.z),
						cube.Position + glm::vec3( h.x, -h.y,  h.z),
						cube.Position + glm::vec3(-h.x, -h.y,  h.z),
						cube.Position + glm::vec3(-h.x,  h.y, -h.z),
						cube.Position + glm::vec3( h.x,  h.y, -h.z),
						cube.Position + glm::vec3( h.x,  h.y,  h.z),
						cube.Position + glm::vec3(-h.x,  h.y,  h.z)
					};

					for (uint8_t index : kCubeEdgeIndices)
						targetBucketVec.emplace_back(corners[index], entry->Color);
				},
				[&](const DebugDrawSphere sphere) {}
			}, entry->Primitive);

			// TODO: If the entry has a duration, add it to the persistent list
			//if (entry->Duration > 0.0f)
			//	m_Persistent.emplace_back(entry);
		}
	}

}