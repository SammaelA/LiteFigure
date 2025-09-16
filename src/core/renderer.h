#pragma once
#include "figure.h"
#include "fonts.h"

namespace LiteFigure
{
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
  };
}