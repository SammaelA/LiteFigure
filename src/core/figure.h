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

    int2   size = int2(-1,-1);
  };
  using FigurePtr = std::shared_ptr<Figure>;

  struct Grid : public Figure
  {
    FigureType getType() const override { return FigureType::Grid; }

    std::vector<std::vector<FigurePtr>> figures;
  };

  struct Collage : public Figure
  {
    struct Element
    {
      float2  pos_rel = float2(0,0);
      float2 size_rel = float2(1,1);
      FigurePtr figure;
    };

    FigureType getType() const override { return FigureType::Collage; }

    std::vector<Element> elements;
  };

  struct Transform : public Figure
  {
    FigureType getType() const override { return FigureType::Transform; }

    FigurePtr figure;
    float4 crop  = float4(0,0,1,1);
    float2 scale = float2(1,1);
  };

  struct Primitive : public Figure
  {
    virtual FigureType getType() const = 0;
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
  };


  void create_and_save_figure(const Block &blk, const std::string &filename);
}