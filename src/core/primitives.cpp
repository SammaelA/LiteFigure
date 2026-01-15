#include "figure.h"
#include "templates.h"
#include "stb_image.h"
#include <cstdio>
#include <filesystem>

#define TINYEXR_USE_MINIZ      0
#define TINYEXR_USE_STB_ZLIB   1
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

namespace LiteFigure
{
  REGISTER_ENUM(SamplerFilter,
                ([]()
                 { return std::vector<std::pair<std::string, unsigned>>{
                       {"Nearest", (unsigned)LiteImage::Sampler::Filter::NEAREST},
                       {"Linear",  (unsigned)LiteImage::Sampler::Filter::LINEAR},
                   }; })());

  REGISTER_ENUM(AddressMode,
                ([]()
                 { return std::vector<std::pair<std::string, unsigned>>{
                       {"Wrap", (unsigned)LiteImage::Sampler::AddressMode::WRAP},
                       {"Mirror",  (unsigned)LiteImage::Sampler::AddressMode::MIRROR},
                       {"Clamp",  (unsigned)LiteImage::Sampler::AddressMode::CLAMP},
                       {"Border",  (unsigned)LiteImage::Sampler::AddressMode::BORDER},
                       {"MirrorOnce",  (unsigned)LiteImage::Sampler::AddressMode::MIRROR_ONCE},
                   }; })());

  bool load_image(const char *path, const char *ext, float gamma, bool use_tonemap, float2 tonemap_range, 
                  bool monochrome, LiteImage::Image2D<float4> &out)
  {
    if (!std::filesystem::exists(path))
    {
      printf("[load_image] file '%s' doesn't exist\n", path);
      return false;
    }
    if (!std::filesystem::is_regular_file(path))
    {
      printf("[load_image] file '%s' is not a file\n", path);
      return false;
    }

    if (strncmp(ext, "exr", 3) == 0)
    {
      float* exr_data; // width * height * RGBA
      int width       = 0;
      int height      = 0;
      const char* err = nullptr;

      int ret = LoadEXR(&exr_data, &width, &height, path, &err);
      if (ret != TINYEXR_SUCCESS) 
      {
        if (err) 
        {
          printf("[load_image] failed to load image '%s' with TinyEXR: %s\n", path, err);
          delete err;
        }
        else
        {
          printf("[load_image] failed to load image '%s' with TinyEXR\n", path); 
        }
        return false;
      }

      out.resize(width, height);
      for(int y = 0; y < height; y++)
      {
        const int offset1 = (height - y - 1) * width;
        const int offset2 = y * width * 4;
        memcpy((void*)(out.data() + offset1), (void*)(exr_data + offset2), width * sizeof(float) * 4);
      }
      free(exr_data);  
    }
    else if (strncmp(ext, "png", 3) == 0 || strncmp(ext, "jpg", 3) == 0 || strncmp(ext, "jpeg", 4) == 0)
    {
      int w, h, channels;
      auto *data_png = stbi_load_16(path, &w, &h, &channels, 0);
      if (!data_png)
      {
        printf("[load_image] failed to load image '%s' with stb_image\n", path);
      }
      else
      {
        out.resize(w, h);

        float mul = 1.0f / 65535.0f;
        for (int i = 0; i < w * h; i++)
        {
          if (channels == 1)
            out.data()[i] = float4(data_png[i] * mul, data_png[i] * mul, data_png[i] * mul, 1);
          else if (channels == 2)
            out.data()[i] = float4(data_png[2 * i] * mul, data_png[2 * i + 1] * mul, 0, 1);
          else if (channels == 3)
            out.data()[i] = float4(data_png[3 * i] * mul, data_png[3 * i + 1] * mul, data_png[3 * i + 2] * mul, 1);
          else if (channels == 4)
            out.data()[i] = float4(data_png[4 * i] * mul, data_png[4 * i + 1] * mul, data_png[4 * i + 2] * mul, data_png[4 * i + 3] * mul);
        }

        stbi_image_free(data_png);
      }
    }
    else
    {
      out = LiteImage::LoadImage<float4>(path, gamma);
    }
    if (out.width() < 1 || out.height() < 1)
    {
      printf("[load_image] failed to load image '%s' with LiteImage\n", path);
      return false;
    }

    if (monochrome)
    {
      for (int i = 1; i < out.width()*out.height(); i++)
      {
        float v = out.data()[i].x;
        out.data()[i] = float4(v,v,v,1);
      }
    }

    if (use_tonemap)
    {
      float4 min_val = out.data()[0];
      float4 max_val = out.data()[0];
      for (int i = 1; i < out.width()*out.height(); i++)
      {
        min_val = min(min_val, out.data()[i]);
        max_val = max(max_val, out.data()[i]);
      }

      max_val = max(max_val, min_val + 1e-9f);
      float4 range_min = float4(tonemap_range.x, tonemap_range.x, tonemap_range.x, min_val.w);
      float4 range_max = float4(tonemap_range.y, tonemap_range.y, tonemap_range.y, max_val.w);
      float4 range_size = range_max - range_min;

      for (int i = 0; i < out.width()*out.height(); i++)
      {
        out.data()[i] = LiteMath::clamp((out.data()[i] - range_min)/range_size, 0.0f, 1.0f);
      }
    }

    return true;
  }

  int2 Primitive::calculateSize(int2 force_size)
  {
    if (force_size.x > 0 && force_size.y > 0)
      size = force_size;
    return size;
  }

  void Primitive::prepareInstances(int2 pos, std::vector<Instance> &out_instances)
  {
    Instance inst;
    inst.prim = this;
    inst.data.pos = pos;
    inst.data.size = size;
    
    out_instances.push_back(inst);    
  }

  bool PrimitiveImage::load(const Block *blk)
  {
    size = blk->get_ivec2("size", size);
    std::string path = blk->get_string("path", "");
    if (path == "")
    {
      printf("[PrimitiveImage::load] path is empty\n");
      return false;
    }
    std::string ext = path.substr(path.find_last_of('.') + 1);
    bool monochrome = blk->get_bool("monochrome", false);
    float gamma = blk->get_double("gamma", 2.2f);
    int tonemap_param_id = blk->get_id("tonemap_range");
    float2 tonemap_range = float2(0,1);
    if (tonemap_param_id >= 0 && blk->get_type(tonemap_param_id) == Block::ValueType::VEC2)
      tonemap_range = blk->get_vec2(tonemap_param_id);
    else
      tonemap_param_id = -1;

    bool status = load_image(path.c_str(), ext.c_str(), gamma, tonemap_param_id >=0, tonemap_range, monochrome, image);
    if (!status)
      return false;

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

    LiteImage::Sampler::AddressMode address_mode = LiteImage::Sampler::AddressMode::CLAMP;
    address_mode = (LiteImage::Sampler::AddressMode)blk->get_enum("address_mode", (uint32_t)address_mode);
    sampler.filter = (LiteImage::Sampler::Filter)blk->get_enum("filter", (uint32_t)sampler.filter);
    sampler.addressU = (LiteImage::Sampler::AddressMode)blk->get_enum("addressU", (uint32_t)address_mode);
    sampler.addressV = (LiteImage::Sampler::AddressMode)blk->get_enum("addressV", (uint32_t)address_mode);
    sampler.addressW = (LiteImage::Sampler::AddressMode)blk->get_enum("addressW", (uint32_t)address_mode);
    sampler.borderColor = blk->get_vec4("border_color", sampler.borderColor);

    return true;
  }

  bool PrimitiveFill::load(const Block *blk)
  {
    size = blk->get_ivec2("size", size);
    color = blk->get_vec4("color", color);

    if (size.x < 1 || size.y < 1)
    {
      printf("[PrimitiveFill::load] size must be explicitly declared\n");
      return false;
    }

    return true;
  }

  bool Rectangle::load(const Block *blk)
  {
    size = blk->get_ivec2("size", size);
    color = blk->get_vec4("color", color);
    region = blk->get_vec4("region", region);
    thickness = blk->get_double("thickness", thickness);
    thickness_pixel = blk->get_int("thickness_pixel", 0);

    if (size.x < 1 || size.y < 1)
    {
      printf("[Rectangle::load] size must be explicitly declared\n");
      return false;
    }

    if (thickness < 0 || thickness > 0.5)
    {
      printf("[Rectangle::load] thickness must be in [0, 0.5] range\n");
      return false;
    }

    return true;
  }

  bool Line::load(const Block *blk)
  {
    size = blk->get_ivec2("size", size);
    color = blk->get_vec4("color", color);
    thickness = blk->get_double("thickness", thickness);
    thickness_pixel = blk->get_int("thickness_pixel", 0);
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