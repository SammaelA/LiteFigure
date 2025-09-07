#include "figure.h"
#include <cstdio>

namespace LiteFigure
{
  std::shared_ptr<Figure> create_figure(const Block &blk)
  {
    printf("block has %d elements\n", blk.size());

    return nullptr;
  }
}