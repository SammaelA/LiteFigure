#pragma once
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
  
  class Renderer
  {
  public:
    Renderer() = default;
    ~Renderer() = default;

    // renders figure into out image, returns true on success
    void render_instance(const Instance &inst, LiteImage::Image2D<float4> &out) const;

	private:
		void render(const PrimitiveImage &prim, const InstanceData &data, LiteImage::Image2D<float4> &out) const;
		void render(const PrimitiveFill &prim, const InstanceData &data, LiteImage::Image2D<float4> &out) const;
		void render(const Line &prim, const InstanceData &data, LiteImage::Image2D<float4> &out) const;
		void render(const Circle &prim, const InstanceData &data, LiteImage::Image2D<float4> &out) const;
		void render(const Polygon &prim, const InstanceData &data, LiteImage::Image2D<float4> &out) const;
		void render(const Rectangle &prim, const InstanceData &data, LiteImage::Image2D<float4> &out) const;
    void render(const Glyph &prim, const InstanceData &data, LiteImage::Image2D<float4> &out) const;
  };
}