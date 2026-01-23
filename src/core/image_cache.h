#pragma once
#include "LiteMath/Image2d.h"
#include "LiteMath/LiteMath.h"

namespace LiteFigure
{
  struct ImageCache
  {
    // Load image from file or take from cache. Thread-safe, but assumes that the image is loaded only once.
    // Never returns nullptr, if the image is not found, returns pointer to a stub image. 
    static const LiteImage::Image2D<LiteMath::float4> *
    load_image(const char *path, const char *ext, float gamma, bool use_tonemap, LiteMath::float2 tonemap_range, bool monochrome, bool flip_y);
  };
};