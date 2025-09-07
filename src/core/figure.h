#pragma once
#include <memory>
#include "blk/blk.h"

namespace LiteFigure
{
  class Figure
  {

  };

  std::shared_ptr<Figure> create_figure(const Block &blk);
}