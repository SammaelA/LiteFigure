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

  static const float gamma_22_LUT[256] = {
    0.00000000, 0.00000508, 0.00002333, 0.00005692, 0.00010719, 0.00017512, 0.00026154, 0.00036714, 0.00049250, 0.00063818, 0.00080466, 0.00099237, 0.00120174, 0.00143313, 0.00168692, 0.00196342, 
    0.00226295, 0.00258583, 0.00293232, 0.00330270, 0.00369724, 0.00411618, 0.00455975, 0.00502820, 0.00552174, 0.00604059, 0.00658496, 0.00715504, 0.00775103, 0.00837312, 0.00902149, 0.00969633, 
    0.01039780, 0.01112608, 0.01188133, 0.01266372, 0.01347340, 0.01431052, 0.01517524, 0.01606770, 0.01698805, 0.01793643, 0.01891298, 0.01991784, 0.02095113, 0.02201299, 0.02310356, 0.02422294, 
    0.02537128, 0.02654868, 0.02775528, 0.02899119, 0.03025652, 0.03155139, 0.03287592, 0.03423021, 0.03561437, 0.03702851, 0.03847275, 0.03994717, 0.04145189, 0.04298701, 0.04455263, 0.04614884, 
    0.04777575, 0.04943346, 0.05112205, 0.05284163, 0.05459228, 0.05637410, 0.05818718, 0.06003161, 0.06190748, 0.06381487, 0.06575388, 0.06772459, 0.06972708, 0.07176145, 0.07382777, 0.07592612, 
    0.07805659, 0.08021926, 0.08241421, 0.08464151, 0.08690125, 0.08919351, 0.09151835, 0.09387587, 0.09626612, 0.09868920, 0.10114516, 0.10363410, 0.10615607, 0.10871115, 0.11129941, 0.11392093, 
    0.11657578, 0.11926401, 0.12198571, 0.12474095, 0.12752978, 0.13035228, 0.13320851, 0.13609855, 0.13902245, 0.14198029, 0.14497213, 0.14799802, 0.15105805, 0.15415226, 0.15728073, 0.16044351, 
    0.16364067, 0.16687227, 0.17013837, 0.17343904, 0.17677432, 0.18014429, 0.18354900, 0.18698851, 0.19046288, 0.19397217, 0.19751643, 0.20109573, 0.20471012, 0.20835966, 0.21204440, 0.21576440, 
    0.21951972, 0.22331041, 0.22713653, 0.23099812, 0.23489526, 0.23882798, 0.24279635, 0.24680042, 0.25084024, 0.25491586, 0.25902733, 0.26317472, 0.26735806, 0.27157742, 0.27583283, 0.28012437, 
    0.28445206, 0.28881597, 0.29321615, 0.29765264, 0.30212550, 0.30663477, 0.31118050, 0.31576274, 0.32038155, 0.32503696, 0.32972903, 0.33445781, 0.33922333, 0.34402566, 0.34886483, 0.35374090, 
    0.35865391, 0.36360390, 0.36859092, 0.37361502, 0.37867625, 0.38377465, 0.38891026, 0.39408313, 0.39929330, 0.40454082, 0.40982574, 0.41514809, 0.42050793, 0.42590529, 0.43134021, 0.43681275, 
    0.44232295, 0.44787084, 0.45345648, 0.45907989, 0.46474113, 0.47044025, 0.47617727, 0.48195224, 0.48776520, 0.49361620, 0.49950528, 0.50543247, 0.51139782, 0.51740137, 0.52344316, 0.52952322, 
    0.53564161, 0.54179836, 0.54799350, 0.55422709, 0.56049915, 0.56680973, 0.57315888, 0.57954661, 0.58597298, 0.59243803, 0.59894179, 0.60548430, 0.61206560, 0.61868573, 0.62534472, 0.63204262, 
    0.63877946, 0.64555527, 0.65237011, 0.65922399, 0.66611697, 0.67304907, 0.68002034, 0.68703081, 0.69408052, 0.70116950, 0.70829779, 0.71546543, 0.72267245, 0.72991889, 0.73720479, 0.74453017, 
    0.75189508, 0.75929955, 0.76674362, 0.77422731, 0.78175068, 0.78931374, 0.79691654, 0.80455911, 0.81224149, 0.81996371, 0.82772579, 0.83552779, 0.84336973, 0.85125165, 0.85917357, 0.86713554, 
    0.87513758, 0.88317974, 0.89126204, 0.89938451, 0.90754720, 0.91575013, 0.92399334, 0.93227685, 0.94060071, 0.94896494, 0.95736958, 0.96581465, 0.97430020, 0.98282626, 0.99139284, 1.00000000, };

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
      if (!stbi_is_16_bit(path) && std::abs(gamma - 2.2f) < 1e-6f) //happy path, use LUT for gamma compensation
      {
        int w, h, channels;
        auto *data_png = stbi_load(path, &w, &h, &channels, 0);
        if (!data_png)
        {
          printf("[load_image] failed to load image '%s' with stb_image\n", path);
          stbi_image_free(data_png);
          return false;
        }
        else
        {
          out.resize(w, h);
          for (int i = 0; i < w * h; i++)
          {
            if (channels == 1)
              out.data()[i] = float4(gamma_22_LUT[data_png[i]], gamma_22_LUT[data_png[i]], gamma_22_LUT[data_png[i]], 1);
            else if (channels == 2)
              out.data()[i] = float4(gamma_22_LUT[data_png[2 * i]], gamma_22_LUT[data_png[2 * i + 1]], 0, 1);
            else if (channels == 3)
              out.data()[i] = float4(gamma_22_LUT[data_png[3 * i]], gamma_22_LUT[data_png[3 * i + 1]], gamma_22_LUT[data_png[3 * i + 2]], 1);
            else if (channels == 4)
              out.data()[i] = float4(gamma_22_LUT[data_png[4 * i]], gamma_22_LUT[data_png[4 * i + 1]], gamma_22_LUT[data_png[4 * i + 2]], gamma_22_LUT[data_png[4 * i + 3]]);
          }
          stbi_image_free(data_png);
        }
      }
      else
      {
        int w, h, channels;
        auto *data_png = stbi_load_16(path, &w, &h, &channels, 0);
        if (!data_png)
        {
          printf("[load_image] failed to load image '%s' with stb_image\n", path);
          stbi_image_free(data_png);
          return false;
        }
        else
        {
          out.resize(w, h);

          float mul = 1.0f / 65535.0f;
          bool apply_gamma = std::abs(gamma - 1.0f) > 1e-6f;
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

            if (apply_gamma)
              out.data()[i] = float4(powf(out.data()[i].x, gamma), powf(out.data()[i].y, gamma), powf(out.data()[i].z, gamma), out.data()[i].w);
          }
          stbi_image_free(data_png);
        }
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