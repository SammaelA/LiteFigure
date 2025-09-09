#include <cstdio>
#include "figure.h"
#include "ttf_reader.h"

int main(int argc, char *argv[]) 
{
  if (argc > 1 && std::string(argv[1]) == "--read_ttf_debug")
  {
    LiteFigure::read_ttf_debug(argv[2]);
    return 0;
  }
  
  if (argc != 3)
  {
    printf("Usage: %s input.blk <output_image>\n", argv[0]);
    return 1;
  }
  Block blk;
  load_block_from_file(argv[1], blk);
  LiteFigure::create_and_save_figure(blk, argv[2]);
  return 0;
}