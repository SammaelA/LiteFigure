#include "image_cache.h"

#include "stb_image.h"
#define TINYEXR_USE_MINIZ      0
#define TINYEXR_USE_STB_ZLIB   1
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <filesystem>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <memory>

namespace LiteFigure
{
  using namespace LiteMath;

  bool load_image_impl(const char *path, const char *ext, float gamma, bool use_tonemap, float2 tonemap_range, 
                       bool monochrome, bool flip_y, LiteImage::Image2D<float4> &out)
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
        const int offset1 = (flip_y ? (height - y - 1) : y) * width;
        const int offset2 = y * width * 4;
        memcpy((void*)(out.data() + offset1), (void*)(exr_data + offset2), width * sizeof(float) * 4);
      }
      free(exr_data);  
    }
    else if (strncmp(ext, "png", 3) == 0 || strncmp(ext, "jpg", 3) == 0 || strncmp(ext, "jpeg", 4) == 0)
    {
      stbi_set_flip_vertically_on_load(flip_y);
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

          out.data()[i] = float4(powf(out.data()[i].x, gamma), powf(out.data()[i].y, gamma), powf(out.data()[i].z, gamma), out.data()[i].w);
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
        out.data()[i] = clamp((out.data()[i] - range_min)/range_size, 0.0f, 1.0f);
      }
    }

    return true;
  }

	const LiteImage::Image2D<float4> *
	ImageCache::load_image(const char *path, const char *ext, float gamma, bool use_tonemap, float2 tonemap_range, bool monochrome, bool flip_y)
	{
    static std::unordered_map<std::string, std::shared_ptr<LiteImage::Image2D<float4>>> cache;
    static LiteImage::Image2D<float4> stub(16, 16, float4(1,0,1,1));
		static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);

    auto it = cache.find(path);
    if (it != cache.end())
      return it->second.get();

    std::shared_ptr<LiteImage::Image2D<float4>> image = std::make_shared<LiteImage::Image2D<float4>>();
    bool image_loaded = load_image_impl(path, ext, gamma, use_tonemap, tonemap_range, monochrome, flip_y, *image);
    if (!image_loaded)
      image = std::make_shared<LiteImage::Image2D<float4>>(stub);
    
    cache[path] = image;
    return image.get();
  }
}