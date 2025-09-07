#include <cstdio>
#include "figure.h"

int main(int argc, char *argv[]) 
{
  Block blk;
  load_block_from_file(argv[1], blk);
  auto fig = LiteFigure::create_figure(blk);
  return 0;
}