#include "renderer.h"

namespace LiteFigure
{
	static inline float4 alpha_blend(float4 a, float4 b)
	{
		float4 r;
		r.w = a.w + b.w * (1.0f - a.w) + 1e-9f;
		r.x = (a.x * a.w + b.x * b.w * (1.0f - a.w)) / r.w;
		r.y = (a.y * a.w + b.y * b.w * (1.0f - a.w)) / r.w;
		r.z = (a.z * a.w + b.z * b.w * (1.0f - a.w)) / r.w;
		return r;
	}

	static inline float2 to_float2(const float3 &v)
	{
		return float2(v.x, v.y);
	}

	void Renderer::render_instance(const Instance &inst, LiteImage::Image2D<float4> &out) const
	{
		if (!inst.prim)
			return;

		switch (inst.prim->getType())
		{
		case FigureType::PrimitiveImage:
			render(static_cast<const PrimitiveImage &>(*inst.prim), inst.data, out);
			break;
		case FigureType::PrimitiveFill:
			render(static_cast<const PrimitiveFill &>(*inst.prim), inst.data, out);
			break;
		case FigureType::Line:
			render(static_cast<const Line &>(*inst.prim), inst.data, out);
			break;
		case FigureType::Circle:
			render(static_cast<const Circle &>(*inst.prim), inst.data, out);
			break;
		case FigureType::Polygon:
			render(static_cast<const Polygon &>(*inst.prim), inst.data, out);
			break;
		case FigureType::Glyph:
			render(static_cast<const Glyph &>(*inst.prim), inst.data, out);
			break;
		default:
			printf("ERROR: trying to render unrenderable primitve (type %d)\n", (int)(inst.prim->getType()));
			break;
		}
	}

	void Renderer::render(const PrimitiveImage &prim, const InstanceData &instance, LiteImage::Image2D<float4> &out) const
	{
		for (int y = 0; y < prim.size.y; y++)
		{
			for (int x = 0; x < prim.size.x; x++)
			{
				float3 uv = instance.uv_transform * float3(x / float(prim.size.x), y / float(prim.size.y), 1);
				float4 c = prim.image.sample(prim.sampler, float2(uv.x, uv.y));
				uint2 pixel = uint2(x + instance.pos.x, y + instance.pos.y);
				out[pixel] = alpha_blend(c, out[pixel]);
			}
		}
	}

	void Renderer::render(const PrimitiveFill &prim, const InstanceData &instance, LiteImage::Image2D<float4> &out) const
	{
		float4 c = prim.color;
		for (int y = instance.pos.y; y < instance.pos.y + prim.size.y; y++)
		{
			for (int x = instance.pos.x; x < instance.pos.x + prim.size.x; x++)
			{
				out[uint2(x, y)] = alpha_blend(c, out[uint2(x, y)]);
			}
		}
	}

	void Renderer::render(const Line &prim, const InstanceData &instance, LiteImage::Image2D<float4> &out) const
	{
		float2 p0 = to_float2(instance.uv_transform * float3(prim.start.x, prim.start.y, 1));
		float2 p1 = to_float2(instance.uv_transform * float3(prim.end.x, prim.end.y, 1));
		p0 = LiteMath::clamp(p0, float2(0, 0), float2(1, 1));
		p1 = LiteMath::clamp(p1, float2(0, 0), float2(1, 1));

		int w = prim.size.x;
		int h = prim.size.y;
		float x0 = p0.x * w, y0 = p0.y * h;
		float x1 = p1.x * w, y1 = p1.y * h;
		float T = prim.thickness * fmax(w, h);
		for (int y = 0; y < h; ++y)
		{
			for (int x = 0; x < w; ++x)
			{
				float px = x + 0.5f, py = y + 0.5f;
				// Compute projection parameter t
				float dx = x1 - x0, dy = y1 - y0;
				float len2 = dx * dx + dy * dy;
				float t = len2 > 0 ? ((px - x0) * dx + (py - y0) * dy) / len2 : 0;
				t = fmax(0, fmin(1, t));
				// Closest point on segment
				float cx = x0 + t * dx, cy = y0 + t * dy;
				float dist = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
				if (dist < T / 2.0f)
				{
					// Optional: for anti-aliasing, fade edge
					float alpha = prim.antialiased ? std::min(1.0f, T / 2.0f - dist) : 1;
					float4 c = prim.color * float4(1, 1, 1, alpha);
					uint2 pixel = uint2(x + instance.pos.x, y + instance.pos.y);
					out[pixel] = alpha_blend(c, out[pixel]);
				}
			}
		}
	}

	void Renderer::render(const Circle &prim, const InstanceData &instance, LiteImage::Image2D<float4> &out) const
	{
		float2 scale = float2(prim.size.x, prim.size.y) / fmax(prim.size.x, prim.size.y);
		float2 p0 = to_float2(instance.uv_transform * float3(prim.center.x - prim.radius, prim.center.y - prim.radius, 1));
		float2 p1 = to_float2(instance.uv_transform * float3(prim.center.x + prim.radius, prim.center.y + prim.radius, 1));
		int min_y = int(floor(fmin(p0.y, p1.y) * prim.size.y));
		int max_y = int(ceil(fmax(p0.y, p1.y) * prim.size.y));
		for (int y = std::max(0, min_y); y < std::min(prim.size.y, max_y); y++)
		{
			for (int x = 0; x < prim.size.x; x++)
			{
				float2 p = to_float2(instance.uv_transform * float3(x / (prim.size.x + 0.5), y / (prim.size.y + 0.5), 1));
				float dist = LiteMath::length(scale * (p - prim.center));
				if (dist <= prim.radius)
				{
					// Optional: for anti-aliasing, fade edge
					float alpha = prim.antialiased ? std::min(1.0f, std::max(prim.size.x, prim.size.y) * (prim.radius - dist)) : 1;
					float4 c = prim.color * float4(1, 1, 1, alpha);
					uint2 pixel = uint2(x + instance.pos.x, y + instance.pos.y);
					out[pixel] = alpha_blend(c, out[pixel]);
				}
			}
		}
	}

	struct Triangle
	{
		float2 p1, p2, p3;

		Triangle(const float2 &a, const float2 &b, const float2 &c) : p1(a), p2(b), p3(c) {}
	};

	class PolygonTriangulator
	{
	public:
		// Calculate signed area of polygon (positive = counter-clockwise, negative = clockwise)
		static float signedArea(const std::vector<float2> &points)
		{
			float area = 0.0f;
			size_t n = points.size();
			for (size_t i = 0; i < n; i++)
			{
				size_t j = (i + 1) % n;
				area += (points[j].x - points[i].x) * (points[j].y + points[i].y);
			}
			return area * 0.5f;
		}

		// Check if point is inside triangle using barycentric coordinates
		static bool pointInTriangle(const float2 &p, const float2 &a, const float2 &b,
																const float2 &c, float eps = 1e-10f)
		{
			float denom = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
			if (std::abs(denom) < 1e-10f)
				return false; // Degenerate triangle

			float alpha = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / denom;
			float beta = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / denom;
			float gamma = 1.0f - alpha - beta;

			return alpha > eps && beta > eps && gamma > eps;
		}

		// Calculate cross product for orientation test
		static float crossProduct(const float2 &o, const float2 &a, const float2 &b)
		{
			return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
		}

		// Check if triangle formed by three consecutive vertices is an ear
		static bool isEar(const std::vector<float2> &points, int prev, int curr, int next)
		{
			const float2 &a = points[prev];
			const float2 &b = points[curr];
			const float2 &c = points[next];

			// Check if triangle is oriented correctly (counter-clockwise)
			if (crossProduct(a, b, c) <= 0)
			{
				return false; // Reflex vertex or degenerate
			}

			// Check if any other vertex lies inside the triangle
			for (size_t i = 0; i < points.size(); i++)
			{
				if (i == prev || i == curr || i == next)
					continue;
				if (pointInTriangle(points[i], a, b, c))
				{
					return false;
				}
			}

			return true;
		}

		// Find closest point on boundary to connect hole
		static size_t findClosestVertex(const std::vector<float2> &boundary, const float2 &holeVertex)
		{
			size_t closest = 0;
			float minDist = std::numeric_limits<float>::max();

			for (size_t i = 0; i < boundary.size(); i++)
			{
				float dx = boundary[i].x - holeVertex.x;
				float dy = boundary[i].y - holeVertex.y;
				float dist = dx * dx + dy * dy;

				if (dist < minDist)
				{
					minDist = dist;
					closest = i;
				}
			}

			return closest;
		}

		// Connect hole to outer boundary
		static std::vector<float2> connectHole(const std::vector<float2> &boundary,
																					 const std::vector<float2> &hole)
		{
			if (hole.empty())
				return boundary;

			// Find rightmost point of hole (good heuristic for connection)
			size_t rightmostHole = 0;
			for (size_t i = 1; i < hole.size(); i++)
			{
				if (hole[i].x > hole[rightmostHole].x)
				{
					rightmostHole = i;
				}
			}

			// Find closest boundary vertex
			size_t closestBoundary = findClosestVertex(boundary, hole[rightmostHole]);

			// Create new polygon with bridge connection
			std::vector<float2> result;

			// Add boundary points up to connection point
			for (size_t i = 0; i <= closestBoundary; i++)
			{
				result.push_back(boundary[i]);
			}

			// Add hole points starting from rightmost
			for (size_t i = 0; i < hole.size(); i++)
			{
				size_t idx = (rightmostHole + i) % hole.size();
				result.push_back(hole[idx]);
			}

			// Close hole by returning to connection point
			result.push_back(hole[rightmostHole]);
			result.push_back(boundary[closestBoundary]);

			// Add remaining boundary points
			for (size_t i = closestBoundary + 1; i < boundary.size(); i++)
			{
				result.push_back(boundary[i]);
			}

			return result;
		}

	public:
		static std::vector<Triangle> triangulateSimple(std::vector<float2> points)
		{
			std::vector<Triangle> triangles;

			if (points.size() < 3)
				return triangles;

			// Ensure counter-clockwise orientation
			if (signedArea(points) > 0)
			{
				std::reverse(points.begin(), points.end());
			}

			// Create vertex index list
			std::vector<int> vertices;
			for (int i = 0; i < static_cast<int>(points.size()); i++)
			{
				vertices.push_back(i);
			}

			// Ear clipping algorithm
			while (vertices.size() > 3)
			{
				bool earFound = false;

				for (size_t i = 0; i < vertices.size(); i++)
				{
					int prev = vertices[(i - 1 + vertices.size()) % vertices.size()];
					int curr = vertices[i];
					int next = vertices[(i + 1) % vertices.size()];

					if (isEar(points, prev, curr, next))
					{
						// Create triangle
						triangles.emplace_back(points[prev], points[curr], points[next]);

						// Remove ear vertex
						vertices.erase(vertices.begin() + i);
						earFound = true;
						break;
					}
				}

				// Safety check to prevent infinite loops
				if (!earFound)
				{
					break;
				}
			}

			// Add final triangle
			if (vertices.size() == 3)
			{
				triangles.emplace_back(points[vertices[0]], points[vertices[1]], points[vertices[2]]);
			}

			return triangles;
		}
	};

	std::vector<Triangle> triangulate(const Polygon &polygon)
	{
		if (polygon.contours.empty())
		{
			return {};
		}

		// Start with outer boundary (first contour)
		std::vector<float2> combinedPolygon = polygon.contours[0].points;

		// Connect all holes to the boundary
		for (size_t i = 1; i < polygon.contours.size(); i++)
		{
			combinedPolygon = PolygonTriangulator::connectHole(combinedPolygon, polygon.contours[i].points);
		}

		// Triangulate the resulting simple polygon
		return PolygonTriangulator::triangulateSimple(combinedPolygon);
	}

	void render_triangle(const Triangle &tri, const InstanceData &instance,
											 LiteImage::Image2D<float4> &out, const float4 &color,
											 int2 size)
	{
		int w = size.x;
		int h = size.y;

		// Transform triangle vertices to pixel coordinates
		float2 p0 = to_float2(instance.uv_transform * float3(tri.p1.x, tri.p1.y, 1));
		float2 p1 = to_float2(instance.uv_transform * float3(tri.p2.x, tri.p2.y, 1));
		float2 p2 = to_float2(instance.uv_transform * float3(tri.p3.x, tri.p3.y, 1));

		p0 = LiteMath::clamp(p0, float2(0, 0), float2(1, 1));
		p1 = LiteMath::clamp(p1, float2(0, 0), float2(1, 1));
		p2 = LiteMath::clamp(p2, float2(0, 0), float2(1, 1));

		int x0 = int(p0.x * w);
		int y0 = int(p0.y * h);
		int x1 = int(p1.x * w);
		int y1 = int(p1.y * h);
		int x2 = int(p2.x * w);
		int y2 = int(p2.y * h);

		// Compute bounding box of the triangle
		int minX = std::max(0, std::min({x0, x1, x2}));
		int maxX = std::min(w - 1, std::max({x0, x1, x2}) + 1);
		int minY = std::max(0, std::min({y0, y1, y2}));
		int maxY = std::min(h - 1, std::max({y0, y1, y2}) + 1);

		// Rasterize triangle using barycentric coordinates
		for (int y = minY; y <= maxY; y++)
		{
			for (int x = minX; x <= maxX; x++)
			{
				float px = x + 0.5f;
				float py = y + 0.5f;

				if (PolygonTriangulator::pointInTriangle(float2(px / w, py / h), p0, p1, p2, -1e-6f))
				{
					uint2 pixel = uint2(x + instance.pos.x, y + instance.pos.y);
					out[pixel] = alpha_blend(color, out[pixel]);
				}
			}
		}
	}

	void Renderer::render(const Polygon &prim, const InstanceData &instance, LiteImage::Image2D<float4> &out) const
	{
		std::vector<Triangle> triangles = triangulate(prim);

		if (prim.outline)
		{
			for (const auto &tri : triangles)
			{
				std::pair<float2, float2> edges[3] = {
						{tri.p1, tri.p2},
						{tri.p2, tri.p3},
						{tri.p3, tri.p1}};
				for (const auto &edge : edges)
				{
					Line line;
					line.start = edge.first;
					line.end = edge.second;
					line.thickness = prim.outline_thickness;
					line.color = prim.color;
					line.antialiased = prim.outline_antialiased;
					line.size = prim.size;
					render(line, instance, out);
				}
			}
		}
		else
		{
			for (const auto &tri : triangles)
			{
				render_triangle(tri, instance, out, prim.color, prim.size);
			}
		}
	}
}