#include "renderer.h"
#include "font.h"

namespace LiteFigure
{
	struct GlyphLine
	{
		float2 p0, p1;
	};

	struct GlyphBezier
	{
		float2 p0, p1, p2;
	};

	float2 quadratic_bezier(const GlyphBezier &bez, float t)
	{
		return (1 - t) * (1 - t) * bez.p0 + 2 * (1 - t) * t * bez.p1 + t * t * bez.p2;
	}

	void solve_quadratic(float a, float b, float c, float &x1, float &x2)
	{
		if (std::abs(a) < 1e-9f)
		{
			x1 = -c / b;
			return;
		}

		float d = b * b - 4 * a * c;
		if (d < 0)
			return;
		x1 = (-b - std::sqrt(d)) / (2 * a);
		x2 = (-b + std::sqrt(d)) / (2 * a);
	}

	void render_bezier_glyph_bruteforce(int2 pos, int2 size, float4 color,
																			const std::vector<GlyphLine> &lines,
																			const std::vector<GlyphBezier> &beziers,
																			LiteImage::Image2D<float4> &out_image)
	{
		std::vector<float2> line_y_limits(lines.size());
		std::vector<float2> bezier_y_limits(beziers.size());
		for (int i = 0; i < lines.size(); i++)
		{
			line_y_limits[i].x = std::min(lines[i].p0.y, lines[i].p1.y);
			line_y_limits[i].y = std::max(lines[i].p0.y, lines[i].p1.y);
		}
		for (int i = 0; i < beziers.size(); i++)
		{
			bezier_y_limits[i].x = std::min(beziers[i].p0.y, std::min(beziers[i].p1.y, beziers[i].p2.y));
			bezier_y_limits[i].y = std::max(beziers[i].p0.y, std::max(beziers[i].p1.y, beziers[i].p2.y));
		}

		for (int y = 0; y < size.y; y++)
		{
			for (int x = 0; x < size.x; x++)
			{
				float2 p = float2((x + 0.5f) / size.x, (y + 0.5f) / size.y);
				int intersections = 0;

				for (int i = 0; i < lines.size(); i++)
				{
					if (p.y < line_y_limits[i].x || p.y > line_y_limits[i].y)
						continue;

					// intersect ray y = p.y with lines
					float t = -(lines[i].p0.y - p.y) / (lines[i].p1.y - lines[i].p0.y);
					if (t > 0 && t < 1)
					{
						float x = lines[i].p0.x + (lines[i].p1.x - lines[i].p0.x) * t;
						if (x > p.x)
							intersections++;
					}
				}

				for (int i = 0; i < beziers.size(); i++)
				{
					if (p.y < bezier_y_limits[i].x || p.y > bezier_y_limits[i].y)
						continue;

					// intersect ray y = p.y with bezier
					float a = beziers[i].p0.y - 2 * beziers[i].p1.y + beziers[i].p2.y;
					float b = 2 * (beziers[i].p1.y - beziers[i].p0.y);
					float c = beziers[i].p0.y - p.y;
					float t1 = 1000, t2 = 1000;
					solve_quadratic(a, b, c, t1, t2);
					if (t1 > 0 && t1 < 1)
					{
						float x = quadratic_bezier(beziers[i], t1).x;
						if (x > p.x)
							intersections++;
					}
					if (t2 > 0 && t2 < 1)
					{
						float x = quadratic_bezier(beziers[i], t2).x;
						if (x > p.x)
							intersections++;
					}
				}

				if (intersections % 2)
					out_image[int2(pos.x + x, pos.y + y)] = alpha_blend(color, out_image[int2(pos.x + x, pos.y + y)]);
			}
		}
	}

	void Renderer::render(const Glyph &prim, const InstanceData &data, LiteImage::Image2D<float4> &out) const
	{
		const Font &font = get_font(prim.font_name);
		const TTFSimpleGlyph &glyph = font.glyphs[prim.glyph_id];
		float2 sz = float2(glyph.xMax - glyph.xMin, glyph.yMax - glyph.yMin);

		std::vector<GlyphLine> lines;
		std::vector<GlyphBezier> beziers;

		for (int cId = 0; cId < glyph.contours.size(); cId++)
		{
			const TTFSimpleGlyph::Contour &contour = glyph.contours[cId];
			int pId = 0;
			while (pId < contour.points.size())
			{
				const TTFSimpleGlyph::Point &p0 = contour.points[pId];
				const TTFSimpleGlyph::Point &p1 = contour.points[(pId + 1) % contour.points.size()];
				const TTFSimpleGlyph::Point &p2 = contour.points[(pId + 2) % contour.points.size()];
				float2 p0f = float2(float(p0.x - glyph.xMin), float(p0.y - glyph.yMin)) / sz;
				float2 p1f = float2(float(p1.x - glyph.xMin), float(p1.y - glyph.yMin)) / sz;
				float2 p2f = float2(float(p2.x - glyph.xMin), float(p2.y - glyph.yMin)) / sz;

				// flip y axis for correct display
				p0f.y = 1.0f - p0f.y;
				p1f.y = 1.0f - p1f.y;
				p2f.y = 1.0f - p2f.y;

				// quadratic bezier
				if (p0.flags.on_curve && !p1.flags.on_curve && p2.flags.on_curve)
				{
					beziers.push_back(GlyphBezier{p0f, p1f, p2f});
					pId += 2;
				}
				else if (p0.flags.on_curve && p1.flags.on_curve)
				{
					lines.push_back(GlyphLine{p0f, p1f});
					pId += 1;
				}
				else
				{
					printf("erroneus flags combination %d %d %d\n", p0.flags.on_curve, p1.flags.on_curve, p2.flags.on_curve);
					pId += 1;
				}
			}
		}

		// printf("render glyph %s %d, size %dx%d, pos %dx%d\n", prim.font_name.c_str(), prim.glyph_id,
		// 	data.size.x, data.size.y, data.pos.x, data.pos.y);
		render_bezier_glyph_bruteforce(data.pos, data.size, prim.color, lines, beziers, out);
	}
}