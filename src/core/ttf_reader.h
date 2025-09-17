#pragma once
#include <string>
#include "font.h"

namespace LiteFigure
{
  Font read_ttf(const std::string &filename);
  bool read_ttf_debug(const std::string &filename);
}