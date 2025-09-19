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
    style_pattern = blk->get_vec2("style_pattern", style_pattern);
    style = (LineStyle)blk->get_enum("style", (uint32_t)style);

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
  
  bool Polygon::load(const Block *blk)
  {
    std::shared_ptr<Polygon> prim = std::make_shared<Polygon>();
    size = blk->get_ivec2("size", size);
    color = blk->get_vec4("color", color);
    outline = blk->get_bool("outline", outline);
    outline_thickness = blk->get_double("outline_thickness", outline_thickness);
    outline_antialiased = blk->get_bool("outline_antialiased", outline_antialiased);
    Block *points_blk = blk->get_block("points");
    Block *contours_blk = blk->get_block("contours");
    if (!points_blk && !contours_blk)
    {
      printf("[Polygon::load] both points and contours blocks are missing\n");
      return false;
    }
    if (points_blk && contours_blk)
    {
      printf("[Polygon::load] only one of points or contours blocks should be present\n");
      return false;
    }

    if (points_blk)
    {
      Contour c;
      for (int i=0;i<points_blk->size();i++)
      {
        if (points_blk->get_type(i) != Block::ValueType::VEC2)
        {
          printf("[Polygon::load] points must be of type vec2\n");
          return false;
        }
        c.points.push_back(points_blk->get_vec2(i, float2(0,0)));
      }
      contours = {c};
    }
    else if (contours_blk)
    {
      for (int i=0;i<contours_blk->size();i++)
      {
        if (contours_blk->get_type(i) != Block::ValueType::BLOCK)
        {
          printf("[Polygon::load] contours must be of type block\n");
          return false;
        }
        Block *cblk = contours_blk->get_block(i);
        Contour c;
        for (int j=0;j<cblk->size();j++)
        {
          if (cblk->get_type(j) != Block::ValueType::VEC2)
          {
            printf("[Polygon::load] contour points must be of type vec2\n");
            return false;
          }
          c.points.push_back(cblk->get_vec2(j, float2(0,0)));
        }
        contours.push_back(c);
      }
    }

    if (contours.empty())
    {
      printf("[Polygon::load] no contours found\n");
      return false;
    }

    for (auto &c : contours)
    {
      if (c.points.size() < 3)
      {
        printf("[Polygon::load] each contour must have at least 3 points\n");
        return false;
      }
    }

    return true;
  }
}