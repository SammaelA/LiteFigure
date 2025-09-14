#include "figure.h"

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

	void PrimitiveImage::render(const InstanceData &instance, LiteImage::Image2D<float4> &out) const
	{
		for (int y = 0; y < size.y; y++)
		{
			for (int x = 0; x < size.x; x++)
			{
				float3 uv = instance.uv_transform * float3(x / float(size.x), y / float(size.y), 1);
				float4 c = image.sample(sampler, float2(uv.x, uv.y));
				uint2 pixel = uint2(x + instance.pos.x, y + instance.pos.y);
				out[pixel] = alpha_blend(c, out[pixel]);
			}
		}
	}

	void PrimitiveFill::render(const InstanceData &instance, LiteImage::Image2D<float4> &out) const
	{
		float4 c = color;
		for (int y = instance.pos.y; y < instance.pos.y + size.y; y++)
		{
			for (int x = instance.pos.x; x < instance.pos.x + size.x; x++)
			{
				out[uint2(x, y)] = alpha_blend(c, out[uint2(x, y)]);
			}
		}
	}

	void Line::render(const InstanceData &instance, LiteImage::Image2D<float4> &out) const
	{
		float2 p0 = to_float2(instance.uv_transform * float3(start.x, start.y, 1));
		float2 p1 = to_float2(instance.uv_transform * float3(end.x, end.y, 1));
		p0 = LiteMath::clamp(p0, float2(0, 0), float2(1, 1));
		p1 = LiteMath::clamp(p1, float2(0, 0), float2(1, 1));

		int w = size.x;
		int h = size.y;
		float x0 = p0.x * w, y0 = p0.y * h;
		float x1 = p1.x * w, y1 = p1.y * h;
		float T = thickness * fmax(w, h);
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
					float alpha = antialiased ? std::min(1.0f, T / 2.0f - dist) : 1;
					float4 c = color * float4(1, 1, 1, alpha);
					uint2 pixel = uint2(x + instance.pos.x, y + instance.pos.y);
					out[pixel] = alpha_blend(c, out[pixel]);
				}
			}
		}
	}

	void Circle::render(const InstanceData &instance, LiteImage::Image2D<float4> &out) const
	{
		float2 p0 = to_float2(instance.uv_transform * float3(center.x - radius, center.y - radius, 1));
		float2 p1 = to_float2(instance.uv_transform * float3(center.x + radius, center.y + radius, 1));
		int min_y = int(floor(fmin(p0.y, p1.y) * size.y));
		int max_y = int(ceil(fmax(p0.y, p1.y) * size.y));
		for (int y = std::max(0, min_y); y < std::min(size.y, max_y); y++)
		{
			for (int x = 0; x < size.x; x++)
			{
				float2 p = to_float2(instance.uv_transform * float3(x / (size.x + 0.5), y /(size.y + 0.5), 1));
				float dist = LiteMath::length(p - center);
				if (dist <= radius)
				{
					// Optional: for anti-aliasing, fade edge
					float alpha = antialiased ? std::min(1.0f, std::max(size.x,size.y)*(radius - dist)) : 1;
					float4 c = color * float4(1, 1, 1, alpha);
					uint2 pixel = uint2(x + instance.pos.x, y + instance.pos.y);
					out[pixel] = alpha_blend(c, out[pixel]);
				}
			}
		}
	}
}