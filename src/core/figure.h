#pragma once
#include <memory>
#include <vector>
#include <string>

#include "LiteMath/Image2d.h"
#include "blk/blk.h"

namespace LiteFigure
{
  enum class FigureType
  {
    Unknown,
    Grid,
    Collage,
    TemplateInstance,
    Transform,
    PrimitiveImage,
    PrimitiveFill
  };
  
  struct Figure
  {
    virtual FigureType getType() const = 0;

    // calculates and returns the size of the figure, also sets size variable
    // if force_size is not -1,-1, it will be used to change the size of the 
    // figure and all it's children accordingly. 
    // Even in this case returned size might be different from force_size
    // due to rounding errors or other constaints
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) = 0;

    int2   size = int2(-1,-1);
  };
  using FigurePtr = std::shared_ptr<Figure>;

  struct Grid : public Figure
  {
    FigureType getType() const override { return FigureType::Grid; }
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;

    std::vector<std::vector<FigurePtr>> rows;
  };

  struct Collage : public Figure
  {
    struct Element
    {
      int2 pos  = int2(0,0);
      int2 size = int2(-1,-1);
      FigurePtr figure;
    };

    FigureType getType() const override { return FigureType::Collage; }
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;

    std::vector<Element> elements;
  };

  struct Transform : public Figure
  {
    FigureType getType() const override { return FigureType::Transform; }
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;

    FigurePtr figure;
    float4 crop  = float4(0,0,1,1);
    float2 scale = float2(1,1);
  };

  struct Primitive : public Figure
  {
    virtual FigureType getType() const = 0;
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override
    {
      if (force_size.x > 0 && force_size.y > 0)
        size = force_size;
      return size;
    }
  };

  struct PrimitiveImage : public Primitive
  {
    FigureType getType() const override { return FigureType::PrimitiveImage; }

    LiteImage::Sampler sampler;
    LiteImage::Image2D<float4> image;
  };

  struct PrimitiveFill : public Primitive
  {
    FigureType getType() const override { return FigureType::PrimitiveFill; }

    float4 color = float4(1,0,0,1);
  };

  struct Instance
  {
    std::shared_ptr<Primitive> prim;
    int2 pos  = int2(0,0);
    int2 size = int2(-1,-1);
    LiteMath::float3x3 uv_transform = LiteMath::float3x3();
  };


  void create_and_save_figure(const Block &blk, const std::string &filename);
}