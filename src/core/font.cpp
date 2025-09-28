#include "font.h"
#include "ttf_reader.h"
#include "renderer.h"
#include "LiteMath/Image2d.h"
#include <map>
#include <fstream>
#include <filesystem>

namespace LiteFigure
{
  std::string font_name_to_sdf_cache_name(const std::string &filename)
  {
    return "cache/" + filename.substr(0, filename.find_last_of(".")) + ".sdf";
  }

  GlyphSDF float_texture_to_GlyphSDF(const LiteImage::Image2D<float> &tex, int max_radius)
  {
    GlyphSDF g;
    g.width = tex.width();
    g.height = tex.height();
    g.data.resize(tex.width() * tex.height());

    float mult = 127.0f/max_radius;
    for (int i=0;i<tex.width()*tex.height();i++)
    {
      float raw_val = mult*tex.data()[i];
      g.data[i] = std::max<int>(INT8_MIN, std::min<int>(INT8_MAX, round(raw_val)));
    }
    return g;
  }

  void save_sdf_glyphs(const std::string &filename, const Font &font)
  {
    std::string path = font_name_to_sdf_cache_name(filename);
    std::ofstream fs(path, std::ios::binary);
    uint32_t num_glyphs = font.glyphs_sdf.size();
    fs.write((const char*)&num_glyphs, sizeof(num_glyphs));
    for (auto &glyph : font.glyphs_sdf)
    {
      fs.write((const char*)&glyph.height, sizeof(glyph.height));
      fs.write((const char*)&glyph.width, sizeof(glyph.width));
      fs.write((const char*)glyph.data.data(), glyph.height * glyph.width);
    }
    fs.flush();
    fs.close();
  }

  void create_sdf_glyphs(Font &font, bool only_ASCII = true)
  {
    constexpr int base_size  = 128;
    constexpr int base_scale = 4;
    constexpr int max_radius = 4;

    font.glyphs_sdf.resize(font.glyphs.size());
    if (only_ASCII)
    {
      for (int i=0;i<256;i++)
      {
        int glyph_id = font.cmap.charGlyphs[i];
        const TTFSimpleGlyph &glyph = font.glyphs[glyph_id];
        printf("preparing SDF glyph %d/%d\n", i, 255);
        if (glyph.contours.size() > 0)
          font.glyphs_sdf[glyph_id] = float_texture_to_GlyphSDF(create_glyph_sdf(base_size, base_scale, max_radius, glyph), max_radius);
      }
    }
    else
    {
      for (int i = 0; i < font.glyphs.size(); i++)
      {
        if (font.glyphs[i].contours.size() > 0)
          font.glyphs_sdf[i] = float_texture_to_GlyphSDF(create_glyph_sdf(base_size, base_scale, max_radius, font.glyphs[i]), max_radius);
      }
    }
  }

  void load_sdf_glyphs(const std::string &filename, Font &font)
  {
    std::string path = font_name_to_sdf_cache_name(filename);
    //if file doesn't exist, create glyphs and save them to file
    if (!std::filesystem::exists(path))
    {
      create_sdf_glyphs(font);
      save_sdf_glyphs(filename, font);
    }
    else
    {
      uint32_t num_glyphs;
      std::ifstream fs(path, std::ios::binary);
      fs.read((char*)&num_glyphs, sizeof(num_glyphs));
      font.glyphs_sdf.resize(num_glyphs);
      for (auto &glyph : font.glyphs_sdf)
      {
        fs.read((char*)&glyph.height, sizeof(glyph.height));
        fs.read((char*)&glyph.width, sizeof(glyph.width));
        glyph.data.resize(glyph.height * glyph.width);
        if (glyph.height * glyph.width > 0)
          fs.read((char*)glyph.data.data(), glyph.height * glyph.width);
      }
      fs.close();
    }
  }

  const Font &get_font(const std::string &filename)
  {
    static std::map<std::string, Font> font_cache;
    auto it = font_cache.find(filename);
    if (it == font_cache.end())
    {
      font_cache[filename] = read_ttf("fonts/" + filename);
      load_sdf_glyphs(filename, font_cache[filename]);
    }
    return font_cache[filename];
  }
};