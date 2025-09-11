#include "figure.h"
#include "templates.h"
#include <cstdio>

namespace LiteFigure
{
  REGISTER_ENUM(FigureType,
                ([]()
                 { return std::vector<std::pair<std::string, unsigned>>{
                       {"Unknown", (unsigned)FigureType::Unknown},
                       {"Grid", (unsigned)FigureType::Grid},
                       {"Collage", (unsigned)FigureType::Collage},
                       {"Transform", (unsigned)FigureType::Transform},
                       {"PrimitiveImage", (unsigned)FigureType::PrimitiveImage},
                       {"PrimitiveFill", (unsigned)FigureType::PrimitiveFill},
                       {"Line", (unsigned)FigureType::Line},
                       {"Circle", (unsigned)FigureType::Circle},
                   }; })());

  bool is_valid_size(int2 size) { return size.x > 0 && size.y > 0; }
  bool equal(int2 a, int2 b) { return a.x == b.x && a.y == b.y; }

  FigurePtr create_error_figure_dummy()
  {
    std::shared_ptr<PrimitiveFill> prim = std::make_shared<PrimitiveFill>();
    prim->size = int2(64, 64);
    prim->color = float4(1, 0, 1, 1);
    return prim;
  }

  FigurePtr create_figure(const Block *blk)
  {
    FigurePtr fig;
    switch ((FigureType)blk->get_enum("type", (unsigned)FigureType::Unknown))
    {
    case FigureType::Unknown:
      printf("[create_figure] unknown figure type\n");
      fig = create_error_figure_dummy();
      break;
    case FigureType::Grid:
      fig = std::make_shared<Grid>();
      break;
    case FigureType::Collage:
      fig = std::make_shared<Collage>();
      break;
    case FigureType::Transform:
      fig = std::make_shared<Transform>();
      break;
    case FigureType::PrimitiveImage:
      fig = std::make_shared<PrimitiveImage>();
      break;
    case FigureType::PrimitiveFill:
      fig = std::make_shared<PrimitiveFill>();
      break;
    case FigureType::Line:
      fig = std::make_shared<Line>();
      break;
    case FigureType::Circle:
      fig = std::make_shared<Circle>();
      break;
    default:
      printf("[create_figure] unsupported figure type %d\n", (int)blk->get_enum("type", (unsigned)FigureType::Unknown));
      fig = create_error_figure_dummy();
      break;
    }

    fig->load(blk);
    return fig;

    return nullptr;
  }

  void get_elements_min_max(const std::vector<Collage::Element> &elements, int2 &min_val, int2 &max_val)
  {
    assert(elements.size() > 0);
    min_val = elements[0].pos;
    max_val = elements[0].pos + elements[0].size;
    for (int i = 1; i < elements.size(); i++)
    {
      min_val = min(min_val, elements[i].pos);
      max_val = max(max_val, elements[i].pos + elements[i].size);
      printf("element %d %d %d %d\n", elements[i].pos.x, elements[i].pos.y, elements[i].size.x, elements[i].size.y);
    }
  }

  int2 Collage::calculateSize(int2 force_size)
  {
    // external force_size has highest priority, but if
    // figure size is explicitly set, it is forced to children
    if (!is_valid_size(force_size) && is_valid_size(size))
      force_size = size;

    // calculate proper size
    int2 min_val, max_val;
    get_elements_min_max(elements, min_val, max_val);
    int2 proper_size = max_val - min_val;

    if (!is_valid_size(force_size))
      force_size = proper_size;

    printf("collage size %d %d force size %d %d proper size %d %d\n", size.x, size.y, force_size.x, force_size.y, proper_size.x, proper_size.y);

    // resize all children
    float2 scale = float2(force_size.x / float(proper_size.x), force_size.y / float(proper_size.y));
    for (int i = 0; i < elements.size(); i++)
    {
      elements[i].pos = int2(float2(elements[i].pos) * scale);
      elements[i].size = elements[i].figure->calculateSize(int2(float2(elements[i].size) * scale));
    }

    // recalculate overall size
    get_elements_min_max(elements, min_val, max_val);
    size = max_val - min_val;

    return size;
  }

  bool Collage::load(const Block *blk)
  {
    size = blk->get_ivec2("size", size);
    for (int i = 0; i < blk->size(); i++)
    {
      Block *elem_blk = blk->get_block(i);
      if (elem_blk)
      {
        Collage::Element elem;
        elem.pos = elem_blk->get_ivec2("pos", elem.pos);
        elem.size = elem_blk->get_ivec2("size", elem.size);
        elem.figure = create_figure(elem_blk);
        elements.push_back(elem);
      }
    }

    if (elements.size() == 0)
    {
      printf("[Collage::load] collage is empty\n");
      return false;
    }

    return true;
  }

  int2 Grid::calculateSize(int2 force_size)
  {
    // external force_size has highest priority, but if
    // figure size is explicitly set, it is forced to children
    if (!is_valid_size(force_size) && is_valid_size(size))
      force_size = size;

    int2 min_pos(0, 0); // always start from 0 for grid
    int2 max_pos(-1, -1);
    int2 cur_pos = int2(0, 0);
    for (auto &row : rows)
    {
      cur_pos.x = 0;
      int row_height = 0;
      for (auto &figure : row)
      {
        int2 figure_size = figure->calculateSize();
        row_height = std::max(row_height, figure_size.y);
        max_pos = max(max_pos, cur_pos + figure_size);
        cur_pos.x += figure_size.x;
      }
      cur_pos.y = row_height;
    }

    int2 proper_size = max_pos - min_pos;
    if (!is_valid_size(force_size) || equal(force_size, proper_size))
    {
      size = proper_size;
      return size;
    }

    float2 scale = float2(force_size.x / float(proper_size.x), force_size.y / float(proper_size.y));
    min_pos = int2(0, 0);
    max_pos = int2(-1, -1);
    cur_pos = int2(0, 0);
    for (auto &row : rows)
    {
      cur_pos.x = 0;
      int row_height = 0;
      for (auto &figure : row)
      {
        int2 figure_size = figure->calculateSize(int2(float2(figure->size) * scale));
        row_height = std::max(row_height, figure_size.y);
        max_pos = max(max_pos, cur_pos + figure_size);
        cur_pos.x += figure_size.x;
      }
      cur_pos.y = row_height;
    }

    size = max_pos - min_pos;
    return size;
  }

  bool Grid::load(const Block *blk)
  {
    size = blk->get_ivec2("size", size);
    for (int i = 0; i < blk->size(); i++)
    {
      Block *row_blk = blk->get_block(i);
      if (!row_blk)
        continue;

      rows.emplace_back();

      for (int j = 0; j < row_blk->size(); j++)
      {
        Block *elem_blk = row_blk->get_block(j);
        if (elem_blk)
          rows.back().push_back(create_figure(elem_blk));
      }

      if (rows.back().size() == 0)
      {
        printf("[Grid::load] row is empty\n");
        return false;
      }
    }

    if (rows.size() == 0)
    {
      printf("[Grid::load] grid is empty\n");
      return false;
    }

    return true;
  }

  int2 Transform::calculateSize(int2 force_size)
  {
    // external force_size has highest priority, but if
    // figure size is explicitly set, it is forced to children
    if (!is_valid_size(force_size) && is_valid_size(size))
      force_size = size;

    if (!is_valid_size(force_size))
      force_size = int2(scale * float2(crop.z - crop.x, crop.w - crop.y) * float2(figure->calculateSize()));

    size = figure->calculateSize(force_size);
    return size;
  }

  bool Transform::load(const Block *blk)
  {
    std::shared_ptr<Transform> transform = std::make_shared<Transform>();
    size = blk->get_ivec2("size", size);
    crop = blk->get_vec4("crop", crop);
    scale = blk->get_vec2("scale", scale);

    int figure_block_id = -1;
    for (int i = 0; i < blk->size(); i++)
    {
      Block *fig_blk = blk->get_block(i);
      if (fig_blk)
      {
        if (figure_block_id == -1)
          figure_block_id = i;
        else
        {
          printf("[Transform::load] only one figure block allowed for transform\n");
          return false;
        }
      }
    }

    if (figure_block_id == -1)
    {
      printf("[Transform::load] transform must contain block with figure\n");
      return false;
    }
    else
    {
      figure = create_figure(blk->get_block(figure_block_id));
    }

    return true;
  }

  void prepare_instances_rec(FigurePtr figure, int2 pos, std::vector<Instance> &instances)
  {
    if (dynamic_cast<Primitive *>(figure.get()) && is_valid_size(figure->size))
    {
      Instance inst;
      inst.prim = std::dynamic_pointer_cast<Primitive>(figure);
      inst.data.pos = pos;
      inst.data.size = figure->size;
      instances.push_back(inst);
    }
    else if (dynamic_cast<Collage *>(figure.get()))
    {
      Collage *collage = dynamic_cast<Collage *>(figure.get());
      for (int i = 0; i < collage->elements.size(); i++)
      {
        prepare_instances_rec(collage->elements[i].figure, pos + collage->elements[i].pos, instances);
      }
    }
    else if (dynamic_cast<Grid *>(figure.get()))
    {
      Grid *grid = dynamic_cast<Grid *>(figure.get());
      int2 cur_pos = int2(0, 0); // local position in grid
      for (auto &row : grid->rows)
      {
        cur_pos.x = 0;
        int row_height = 0;
        for (auto &figure : row)
        {
          row_height = std::max(row_height, figure->size.y);
          prepare_instances_rec(figure, pos + cur_pos, instances);
          cur_pos.x += figure->size.x;
        }
        cur_pos.y += row_height;
      }
    }
    else if (dynamic_cast<Transform *>(figure.get()))
    {
      Transform *transform = dynamic_cast<Transform *>(figure.get());

      auto child_type = transform->figure->getType();
      if (child_type == FigureType::PrimitiveImage || child_type == FigureType::Transform)
      {
        float3x3 crop_trans;
        {
          float x0 = transform->crop.x;
          float y0 = transform->crop.y;
          float x1 = transform->crop.z;
          float y1 = transform->crop.w;
          crop_trans = float3x3(x1 - x0, 0, x0, 0, y1 - y0, y0, 0, 0, 1);
        }
        std::vector<Instance> instances_to_transform;
        prepare_instances_rec(transform->figure, pos, instances_to_transform);
        for (auto &inst : instances_to_transform)
        {
          inst.data.uv_transform = crop_trans * inst.data.uv_transform;
          instances.push_back(inst);
        }
      }
      else
      {
        printf("[Transform::prepare_instances] only PrimitiveImage can be transformed now\n");
        return;
      }
    }
  }

  std::vector<Instance> prepare_instances(FigurePtr figure)
  {
    int2 actual_size = figure->calculateSize(figure->size);
    printf("actual size %d %d\n", actual_size.x, actual_size.y);
    std::vector<Instance> instances;
    prepare_instances_rec(figure, int2(0, 0), instances);
    return instances;
  }

  void create_and_save_figure(const Block &blk, const std::string &filename)
  {
    Block temp_blk;
    const Block *figure_blk = nullptr;
    if (blk.get_block("templates") && blk.get_block("figure"))
    {
      const Block *templates_blk = blk.get_block("templates");
      std::map<std::string, const Block *> templates_lib;
      for (int i = 0; i < templates_blk->size(); i++)
      {
        if (templates_blk->get_block(i))
          templates_lib[templates_blk->get_name(i)] = templates_blk->get_block(i);
      }
      temp_blk.copy(blk.get_block("figure"));
      instantiate_all_templates(&temp_blk, templates_lib);
      figure_blk = &temp_blk;

      save_block_to_file("saves/temp_1.blk", temp_blk);
    }
    else
    {
      figure_blk = &blk;
    }

    FigurePtr fig = create_figure(figure_blk);

    if (fig->getType() == FigureType::Unknown)
    {
      printf("[create_and_save_figure] top level figure type is unknown, probably invalid blk\n");
      return;
    }

    std::vector<Instance> instances = prepare_instances(fig);

    // if different conversions are available (e.g. save to svg
    // or pdf), they can be selected here
    bool should_render = true;

    if (should_render)
    {
      LiteImage::Image2D<float4> out = LiteImage::Image2D<float4>(fig->size.x, fig->size.y);
      for (auto &inst : instances)
      {
        IRenderable *renderable = dynamic_cast<IRenderable *>(inst.prim.get());
        if (renderable)
          renderable->render(inst.data, out);
        else
          printf("ERROR: trying to render unrenderable primitve (type %d)\n", (int)(inst.prim->getType()));
      }
      LiteImage::SaveImage(filename.c_str(), out);
    }
  }
}