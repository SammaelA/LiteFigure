#pragma once
#include <map>
#include "blk/blk.h"

void instantiate_all_templates(Block *blk, const std::map<std::string, const Block *> &templates_lib);