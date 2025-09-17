#include "font.h"
#include "ttf_reader.h"
#include <map>

namespace LiteFigure
{
  const Font &get_font(const std::string &filename)
  {
    static std::map<std::string, Font> font_cache;
    auto it = font_cache.find(filename);
    if (it == font_cache.end())
    {
      font_cache[filename] = read_ttf(filename);
    }
    return font_cache[filename];
  }
};