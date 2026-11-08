#include "cdlpch.h"
#include "DebugDrawManager.h"

namespace Candle {

	namespace {

		static DebugDrawManager* s_Manager{ nullptr };

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

		std::vector<GPU::DebugDrawVertex> TessellateLine(const DebugDrawLine& line, uint16_t color)
		{
			std::vector<GPU::DebugDrawVertex> vertices;
			vertices.emplace_back(line.Start, color);
			vertices.emplace_back(line.End, color);
			return vertices;
		}

		std::vector<GPU::DebugDrawVertex> TessellatePoint(const DebugDrawPoint& point, uint16_t color)
		{
			auto upVec = TessellateLine({ point.Position - glm::vec3(0.0f, point.Size / 2.0f, 0.0f), point.Position + glm::vec3(0.0f, point.Size / 2.0f, 0.0f) }, color);
			auto rightVec = TessellateLine({ point.Position - glm::vec3(point.Size / 2.0f, 0.0f, 0.0f), point.Position + glm::vec3(point.Size / 2.0f, 0.0f, 0.0f) }, color);
			std::vector<GPU::DebugDrawVertex> vertices;
			vertices.insert(vertices.end(), upVec.begin(), upVec.end());
			vertices.insert(vertices.end(), rightVec.begin(), rightVec.end());
			return vertices;
		}

		std::vector<GPU::DebugDrawVertex> TessellateCube(const DebugDrawCube& cube, uint16_t color)
		{
			std::vector<GPU::DebugDrawVertex> vertices;
			const glm::vec3 h = cube.Scale * 0.5f;
			const std::array<glm::vec3, 8> corners = {
				cube.Position + glm::vec3(-h.x, -h.y, -h.z),
				cube.Position + glm::vec3(h.x, -h.y, -h.z),
				cube.Position + glm::vec3(h.x, -h.y,  h.z),
				cube.Position + glm::vec3(-h.x, -h.y,  h.z),
				cube.Position + glm::vec3(-h.x,  h.y, -h.z),
				cube.Position + glm::vec3(h.x,  h.y, -h.z),
				cube.Position + glm::vec3(h.x,  h.y,  h.z),
				cube.Position + glm::vec3(-h.x,  h.y,  h.z)
			};
			for (uint8_t index : kCubeEdgeIndices)
				vertices.emplace_back(corners[index], color);
			return vertices;
		}

		void AddEntryToBucket(DebugDrawManager::DebugDrawEntry& entry, std::vector<GPU::DebugDrawVertex>& bucketData)
		{
			auto insertVerts = [&](const std::vector<GPU::DebugDrawVertex>& verts) {
				bucketData.insert(bucketData.end(), verts.begin(), verts.end());
			};
			
			std::visit(Overloaded{
				[&](const DebugDrawPoint point) { insertVerts(TessellatePoint(point, entry.Color)); },
				[&](const DebugDrawLine line) {	insertVerts(TessellateLine(line, entry.Color));	},
				[&](const DebugDrawCube cube) {	insertVerts(TessellateCube(cube, entry.Color));	},
				[&](const DebugDrawSphere sphere) {}
				}, entry.Primitive);
		}
	}

	// DebugDraw implementations
#ifdef CDL_DEBUG_DRAW_ENABLED
	namespace DebugDraw {

		void Point(glm::vec3 position, glm::vec4 color, const Options& options)
		{
			DebugDrawPoint point{
				.Position{ position },
				.Size{ kDefaultPointSize }
			};
			SubmitEntry(point, color, options);
		}

		void Point(glm::vec3 position, float size, glm::vec4 color, const Options& options)
		{
			DebugDrawPoint point{
				.Position{ position },
				.Size{ size }
			};
			SubmitEntry(point, color, options);
		}

		void Line(glm::vec3 start, glm::vec3 end, glm::vec4 color, const Options& options)
		{
			DebugDrawLine line{
				.Start{ start },
				.End{ end }
			};
			SubmitEntry(line, color, options);
		}

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

		//	Loop over persistent items and add to buckets if not yet expired
		//  If expired, then swap-remove, otherwise, add to bucket
		for (size_t i = 0; i < m_Persistent.size();)
		{
			auto& entry = m_Persistent[i];
			entry.Duration -= delta;
			if (entry.Duration <= 0.0f)
			{
				// Remove expired entry (swap-remove)
				m_Persistent[i] = m_Persistent.back();
				m_Persistent.pop_back();
			}
			else
			{
				// Add to bucket
				auto& targetBucketVec = outData.Buckets[static_cast<size_t>(entry.Bucket)];
				AddEntryToBucket(entry, targetBucketVec);

				++i;
			}
		}

		// Extract entries from buffer, tessellate and put vertices into proper bucket
		while (auto entry = m_DrawQueue.Pop())
		{
			// If no more entries, break
			if (!entry.has_value())
				break;

			auto& targetBucketVec = outData.Buckets[static_cast<size_t>(entry->Bucket)];
			AddEntryToBucket(*entry, targetBucketVec);

			// If the entry has a duration, add it to the persistent list
			if (entry->Duration > 0.0f)
				m_Persistent.emplace_back(*entry);
		}
	}

}