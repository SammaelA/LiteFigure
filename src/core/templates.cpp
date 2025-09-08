#include "templates.h"

void instantiate_rec(Block *blk, const std::map<std::string, Block::Value *> &values)
{
  for (int i=0;i<blk->size();i++)
  {
    auto it = values.find(blk->get_name(i));
    if (it == values.end() || it->second == nullptr)
      continue;
    
    if (it->second->type != blk->get_type(i))
    {
      printf("type of element %s in template and instantiation does not match\n", blk->get_name(i).c_str());
      continue;
    }

    blk->values[i].clear();
    blk->values[i].copy(*(it->second));
  }

  for (int i=0;i<blk->size();i++)
  {
    if (blk->get_block(i))
      instantiate_rec(blk->get_block(i), values);
  }
}

void instantiate_template_here(Block *blk, const std::map<std::string, const Block *> &templates_lib)
{
  if (!blk->has_tag("is_template"))
    return;

  std::string template_name = blk->get_string("template_name");
  auto template_blk_it = templates_lib.find(template_name);
  if (template_name == "" || templates_lib.find(template_name) == templates_lib.end())
  {
    printf("Template <%s> is not found in templates library\n", template_name.c_str());
    return;
  }

  const Block *template_blk = template_blk_it->second;
  std::vector<std::string> params;
  template_blk->get_arr("__params", params);

  if (params.empty())
  {
    printf("Template <%s> has empty list of parameters\n", template_name.c_str());
    return;
  }

  std::map<std::string, Block::Value *> values;
  for (int i=0;i<blk->size();i++)
  {
    for (auto &param : params)
    {
      if (blk->get_name(i) == param)
        values[param] = &(blk->values[i]);
    }
  }

  Block inst_block;
  inst_block.copy(template_blk);
  instantiate_rec(&inst_block, values);
  instantiate_all_templates(&inst_block, templates_lib);
  blk->clear();
  blk->copy(&inst_block);
}

void instantiate_all_templates(Block *blk, const std::map<std::string, const Block *> &templates_lib)
{
  if (!blk)
    return;
  
  instantiate_template_here(blk, templates_lib);
  
  for (int i=0;i<blk->size();i++)
  {
    if (blk->get_block(i))
      instantiate_all_templates(blk->get_block(i), templates_lib);
  }
}