#include "cdlpch.h"
#include "DebugDrawManager.h"
#include "ConsoleManager.h"
#include "DebugConstants.h"

#include "Candle/Core/Profiler.h"

#include <glm/gtc/packing.hpp>

#include <atomic>
#include <ranges>

namespace Candle {

	namespace {

		static std::atomic<DebugDrawManager*> s_Manager{ nullptr };

		// Cube corner pairs for a line list: bottom ring, top ring, then the four verticals
		constexpr std::array<uint8_t, 24> kCubeEdgeIndices = {
			0, 1,  1, 2,  2, 3,  3, 0,
			4, 5,  5, 6,  6, 7,  7, 4,
			0, 4,  1, 5,  2, 6,  3, 7
		};

		uint8_t QuantizeLineWidth(float width)
		{
			float steps = std::round(width * kLineWidthStepsPerPixel);
			return static_cast<uint8_t>(std::clamp(steps, 1.0f, 255.0f));
		}

		// Bucket is the major key since changing it changes pipeline state, not just the line width
		uint32_t SortKey(const DebugDrawManager::DebugDrawEntry& entry)
		{
			return (static_cast<uint32_t>(entry.Bucket) << 8) | entry.LineWidth;
		}

		void SubmitEntry(DebugDrawPrimitive primitive, glm::vec4 color, const DebugDraw::Options& options)
		{
			DebugDrawManager* manager = s_Manager.load(std::memory_order_acquire);
			if (!manager)
				return;

			DebugDrawBucket bucket = options.DepthTest ? DebugDrawBucket::WorldDepthTested : DebugDrawBucket::WorldNoDepth;
			manager->Submit({
				.Primitive{ primitive },
				.Bucket{ bucket },
				.LineWidth{ QuantizeLineWidth(options.LineWidth) },
				.Color{ glm::packUnorm4x8(color) },
				.Duration{ options.Duration }
			});
		}

		// Must match what each Tessellate function appends
		size_t VertexCount(const DebugDrawPrimitive& primitive)
		{
			return std::visit(Overloaded{
				[](const DebugDrawPoint&) { return size_t{ 4 }; },
				[](const DebugDrawLine&) { return size_t{ 2 }; },
				[](const DebugDrawCircle&) { return kCircleSegmentCount * 2; },
				[](const DebugDrawCube&) { return kCubeEdgeIndices.size(); },
				[](const DebugDrawSphere&) { return kCircleSegmentCount * 2 * 3; }
				}, primitive);
		}

		void TessellateLine(const DebugDrawLine& line, uint32_t color, std::vector<GPU::DebugDrawVertex>& outVertices)
		{
			outVertices.emplace_back(line.Start, color);
			outVertices.emplace_back(line.End, color);
		}

		void TessellatePoint(const DebugDrawPoint& point, uint32_t color, std::vector<GPU::DebugDrawVertex>& outVertices)
		{
			TessellateLine({ point.Position - glm::vec3(0.0f, point.Size / 2.0f, 0.0f), point.Position + glm::vec3(0.0f, point.Size / 2.0f, 0.0f) }, color, outVertices);
			TessellateLine({ point.Position - glm::vec3(point.Size / 2.0f, 0.0f, 0.0f), point.Position + glm::vec3(point.Size / 2.0f, 0.0f, 0.0f) }, color, outVertices);
		}

		// TODO:  Need to support rotations so can draw 3 circles for a sphere
		void TessellateCircle(const DebugDrawCircle& circle, uint32_t color, std::vector<GPU::DebugDrawVertex>& outVertices)
		{
			// Create a rotation matrix to align the circle with the specified plane normal
			glm::vec3 normalizedNormal = glm::normalize(circle.PlaneNormal);
			glm::vec3 reference = glm::abs(normalizedNormal.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
			glm::vec3 u = glm::normalize(glm::cross(reference, normalizedNormal));	// Only unit length when the normal is perpendicular to the reference
			glm::vec3 v = glm::cross(normalizedNormal, u);

			// Parametric equation of a circle
			auto calcRotatedVert([&](float radian) {
				return circle.Center + (circle.Radius * glm::cos(radian) * u) + (circle.Radius * glm::sin(radian) * v);
			});

			// Draw the circle
			float segments = 360.0f / kCircleSegmentCount;
			auto indexToRadians([segments](uint32_t i) { return glm::radians(i * segments); });
			for (uint32_t i = 0; i < kCircleSegmentCount; i++)
			{
				float startRad = indexToRadians(i);
				float endRad = indexToRadians((i + 1) % kCircleSegmentCount);
				TessellateLine({ calcRotatedVert(startRad), calcRotatedVert(endRad) }, color, outVertices);
			}
		}

		void TessellateCube(const DebugDrawCube& cube, uint32_t color, std::vector<GPU::DebugDrawVertex>& outVertices)
		{
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
				outVertices.emplace_back(corners[index], color);
		}

		void TessellateSphere(const DebugDrawSphere& sphere, uint32_t color, std::vector<GPU::DebugDrawVertex>& outVertices)
		{
			// Draw three circles for the sphere
			TessellateCircle({ sphere.Position, glm::vec3(1.0f, 0.0f, 0.0f), sphere.Radius }, color, outVertices);
			TessellateCircle({ sphere.Position, glm::vec3(0.0f, 1.0f, 0.0f), sphere.Radius }, color, outVertices);
			TessellateCircle({ sphere.Position, glm::vec3(0.0f, 0.0f, 1.0f), sphere.Radius }, color, outVertices);
		}

		void TessellateEntry(const DebugDrawManager::DebugDrawEntry& entry, std::vector<GPU::DebugDrawVertex>& outVertices)
		{
			std::visit(Overloaded{
				[&](const DebugDrawPoint& point) { TessellatePoint(point, entry.Color, outVertices); },
				[&](const DebugDrawLine& line) { TessellateLine(line, entry.Color, outVertices); },
				[&](const DebugDrawCircle& circle) { TessellateCircle(circle, entry.Color, outVertices); },
				[&](const DebugDrawCube& cube) { TessellateCube(cube, entry.Color, outVertices); },
				[&](const DebugDrawSphere& sphere) { TessellateSphere(sphere, entry.Color, outVertices); }
				}, entry.Primitive);
		}
	}

	// DebugDraw implementations
#if CDL_DEBUG_DRAW_ENABLED
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

		void Circle(glm::vec3 center, glm::vec3 planeNormal, float radius, glm::vec4 color, const Options& options)
		{
			DebugDrawCircle circle{
				.Center{ center },
				.PlaneNormal{ planeNormal },
				.Radius{ radius }
			};
			SubmitEntry(circle, color, options);
		}

		void Cube(glm::vec3 center, glm::vec3 size, glm::vec4 color, const Options& options)
		{
			DebugDrawCube cube{
				.Position{ center },
				.Scale{ size }
			};
			SubmitEntry(cube, color, options);
		}

		void Sphere(glm::vec3 center, float radius, glm::vec4 color, const Options& options)
		{
			DebugDrawSphere sphere{
				.Position{ center },
				.Radius{ radius }
			};
			SubmitEntry(sphere, color, options);
		}

	}
#endif

	DebugDrawManager::DebugDrawManager(ConsoleManager& consoleManager)
		: m_ConsoleManager(consoleManager)
	{
		CDL_CORE_ASSERT(!s_Manager.load(std::memory_order_relaxed), "DebugDrawManager already exists!");
		s_Manager.store(this, std::memory_order_release);
	}

	DebugDrawManager::~DebugDrawManager()
	{
		s_Manager.store(nullptr, std::memory_order_release);
	}

	void DebugDrawManager::Submit(DebugDrawEntry entry)
	{
		m_DrawQueue.TryPush(entry);
	}

	void DebugDrawManager::ExtractDebugDrawData(DebugDrawData& outData, TimeStep delta)
	{
		CDL_PROFILE_FUNCTION();

		outData.Vertices.clear();
		outData.Batches.clear();
		m_FrameEntries.clear();

		//	Loop over persistent items and keep them for this frame if not yet expired
		//  If expired, then swap-remove
		for (size_t i = 0; i < m_Persistent.size();)
		{
			auto& entry = m_Persistent[i];

			// Decrement duration only if its not unlimited
			if (entry.Duration != DebugDraw::kUnlimitedDuration)
				entry.Duration -= delta;

			if (entry.Duration > 0.0f)
			{
				m_FrameEntries.push_back(entry);
				++i;
			}
			else
			{
				// Remove expired entry (swap-remove)
				m_Persistent[i] = m_Persistent.back();
				m_Persistent.pop_back();
			}
		}

		while (auto entry = m_DrawQueue.Pop())
		{
			m_FrameEntries.push_back(*entry);

			// If the entry has a duration, add it to the persistent list
			if (entry->Duration > 0.0f)
				m_Persistent.push_back(*entry);
		}

		std::ranges::sort(m_FrameEntries, {}, SortKey);

		size_t vertexCount = 0;
		for (const DebugDrawEntry& entry : m_FrameEntries)
			vertexCount += VertexCount(entry.Primitive);
		outData.Vertices.reserve(vertexCount);

		uint32_t batchKey = std::numeric_limits<uint32_t>::max();
		for (const auto& entry : m_FrameEntries)
		{
			uint32_t key = SortKey(entry);
			if (key != batchKey)
			{
				outData.Batches.push_back({
					.Bucket{ entry.Bucket },
					.LineWidth{ entry.LineWidth / kLineWidthStepsPerPixel },
					.FirstVertex{ static_cast<uint32_t>(outData.Vertices.size()) },
					.VertexCount{ 0 }
				});
				batchKey = key;
			}

			TessellateEntry(entry, outData.Vertices);

			DebugDrawBatch& batch = outData.Batches.back();
			batch.VertexCount = static_cast<uint32_t>(outData.Vertices.size()) - batch.FirstVertex;
		}

		CDL_CORE_ASSERT(outData.Vertices.size() == vertexCount, "VertexCount is out of step with the Tessellate functions");
	}

}