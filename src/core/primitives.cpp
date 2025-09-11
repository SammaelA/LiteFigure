#include "figure.h"
#include "templates.h"
#include <cstdio>

namespace LiteFigure
{
  bool PrimitiveImage::load(const Block *blk)
  {
    size = blk->get_ivec2("size", size);
    std::string path = blk->get_string("path", "");
    if (path == "")
    {
      printf("[PrimitiveImage::load] path is empty\n");
      return false;
    }
    image = LiteImage::LoadImage<float4>(path.c_str());
    //TODO: set sampler
    //TODO: support images with alpha
    for (int i=0;i<image.height()*image.width();i++)
      image.data()[i].w = 1.0f;

    if (image.width() < 1 || image.height() < 1)
    {
      printf("[PrimitiveImage::load] image is invalid\n");
      return false;
    }
    else if (size.x < 1 || size.y < 1)
    {
      size = int2(image.width(), image.height());
    }

    return true;
  }

  bool PrimitiveFill::load(const Block *blk)
  {
    std::shared_ptr<PrimitiveFill> prim = std::make_shared<PrimitiveFill>();
    size = blk->get_ivec2("size", size);
    color = blk->get_vec4("color", color);

    if (size.x < 1 || size.y < 1)
    {
      printf("[PrimitiveFill::load] size must be explicitly declared\n");
      return false;
    }

    return true;
  }

  bool Line::load(const Block *blk)
  {
    std::shared_ptr<Line> prim = std::make_shared<Line>();
    size = blk->get_ivec2("size", size);
    color = blk->get_vec4("color", color);
    thickness = blk->get_double("thickness", thickness);
    start = blk->get_vec2("start", start);
    end = blk->get_vec2("end", end);
    antialiased = blk->get_bool("antialiased", antialiased);

    if (size.x < 1 || size.y < 1)
    {
      printf("[Line::load] size must be explicitly declared\n");
      return false;
    }

    return true;
  }

  bool Circle::load(const Block *blk)
  {
    std::shared_ptr<Circle> prim = std::make_shared<Circle>();
    size = blk->get_ivec2("size", size);
    color = blk->get_vec4("color", color);
    radius = blk->get_double("radius", radius);
    center = blk->get_vec2("center", center);
    antialiased = blk->get_bool("antialiased", antialiased);

    if (size.x < 1 || size.y < 1)
    {
      printf("[Circle::load] size must be explicitly declared\n");
      return false;
    }

    return true;
  }
  
}