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
}