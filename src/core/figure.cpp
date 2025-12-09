#include "figure.h"
#include "templates.h"
#include "renderer.h"
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
                       {"Polygon", (unsigned)FigureType::Polygon},
                       {"Text", (unsigned)FigureType::Text},
                       {"Glyph", (unsigned)FigureType::Glyph},
                       {"LinePlot", (unsigned)FigureType::LinePlot},
                       {"LineGraph", (unsigned)FigureType::LineGraph},
                       {"Rectangle", (unsigned)FigureType::Rectangle},
                   }; })());

  REGISTER_ENUM(LineStyle,
                ([]()
                 { return std::vector<std::pair<std::string, unsigned>>{
                       {"Solid", (unsigned)LineStyle::Solid},
                       {"Dashed", (unsigned)LineStyle::Dashed},
                       {"Dotted", (unsigned)LineStyle::Dotted},
                   }; })());

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
    case FigureType::Polygon:
      fig = std::make_shared<Polygon>();
      break;
    case FigureType::Text:
      fig = std::make_shared<Text>();
      break;
    case FigureType::Glyph:
      fig = std::make_shared<Glyph>();
      break;
    case FigureType::LinePlot:
      fig = std::make_shared<LinePlot>();
      break;
    case FigureType::LineGraph:
      fig = std::make_shared<LineGraph>();
      break;
    case FigureType::Rectangle:
      fig = std::make_shared<Rectangle>();
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
    min_val = int2(0, 0);
    max_val = int2(0, 0);
    for (int i = 0; i < elements.size(); i++)
    {
      min_val = min(min_val, elements[i].pos);
      max_val = max(max_val, elements[i].pos + elements[i].size);
    }
  }

  int2 Collage::calculateSize(int2 force_size)
  {
    // external force_size has highest priority, but if
    // figure size is explicitly set, it is forced to children
    if (!is_valid_size(force_size) && is_valid_size(size))
      force_size = size;

    if (elements.size() == 0)
    {
      if (is_valid_size(force_size))
        size = force_size;
      else
        size = int2(1, 1);
      return size;
    }

    // if element size is not set, set it to figure size
    for (int i = 0; i < elements.size(); i++)
    {
      if (!is_valid_size(elements[i].size))
        elements[i].size = elements[i].figure->calculateSize();
    }

    // calculate proper size
    int2 min_val, max_val;
    get_elements_min_max(elements, min_val, max_val);
    int2 proper_size = max_val - min_val;

    if (verbose)
      printf("[Collage] %d elems, min %d %d, max %d %d, proper %d %d\n", (int)elements.size(), min_val.x, min_val.y, max_val.x, max_val.y, proper_size.x, proper_size.y);

    // resize all children
    if (is_valid_size(force_size))
    {
      float2 scale = float2(force_size.x / float(proper_size.x), force_size.y / float(proper_size.y));
      if (verbose)
        printf("[Collage] proper size %d %d, force size %d %d, scale %f %f\n", proper_size.x, proper_size.y, force_size.x, force_size.y, scale.x, scale.y);
      for (int i = 0; i < elements.size(); i++)
      {
        elements[i].pos = int2(float2(elements[i].pos) * scale);
        int2 target_size = max(int2(1, 1), int2(float2(elements[i].size) * scale));
        elements[i].size = elements[i].figure->calculateSize(target_size);
        if (verbose)
          printf("[Collage] element %d (type %d), pos %d %d, target size %d %d -> size %d %d\n", i, (int)elements[i].figure->getType(),
                 elements[i].pos.x, elements[i].pos.y,
                 target_size.x, target_size.y, elements[i].size.x, elements[i].size.y);
      }
    }

    // recalculate overall size
    get_elements_min_max(elements, min_val, max_val);
    size = max_val - min_val;

    if (verbose)
      printf("[Collage] final size %d %d\n", size.x, size.y);

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
        if (verbose)
          printf("[Grid] figure pos %d %d, size %d %d\n", cur_pos.x, cur_pos.y, figure_size.x, figure_size.y);
        row_height = std::max(row_height, figure_size.y);
        max_pos = max(max_pos, cur_pos + figure_size);
        cur_pos.x += figure_size.x;
      }
      cur_pos.y += row_height;
    }

    int2 proper_size = max_pos - min_pos;
    if (!is_valid_size(force_size) || equal(force_size, proper_size))
    {
      size = proper_size;
      return size;
    }

    float2 scale = float2(force_size.x / float(proper_size.x), force_size.y / float(proper_size.y));
    if (verbose)
      printf("[Grid] force size %d %d, proper size %d %d, scale %f %f\n", force_size.x, force_size.y, proper_size.x, proper_size.y, scale.x, scale.y);
    min_pos = int2(0, 0);
    max_pos = int2(-1, -1);
    cur_pos = int2(0, 0);
    for (auto &row : rows)
    {
      cur_pos.x = 0;
      int row_height = 0;
      for (auto &figure : row)
      {
        int2 target_size = max(int2(1, 1), int2(float2(figure->size) * scale));
        int2 figure_size = figure->calculateSize(target_size);
        if (verbose)
          printf("[Grid] figure pos %d %d, target size %d %d, size %d %d\n", cur_pos.x, cur_pos.y,
                 target_size.x, target_size.y, figure_size.x, figure_size.y);
        row_height = std::max(row_height, figure_size.y);
        max_pos = max(max_pos, cur_pos + figure_size);
        cur_pos.x += figure_size.x;
      }
      cur_pos.y += row_height;
    }

    size = max_pos - min_pos;
    return size;
  }

  void Collage::prepareInstances(int2 pos, std::vector<Instance> &out_instances)
  {
    for (int i = 0; i < elements.size(); i++)
      elements[i].figure->prepareInstances(pos + elements[i].pos, out_instances);
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

  void Grid::prepareInstances(int2 pos, std::vector<Instance> &out_instances)
  {
    int2 cur_pos = int2(0, 0); // local position in grid
    for (auto &row : rows)
    {
      cur_pos.x = 0;
      int row_height = 0;
      for (auto &figure : row)
      {
        row_height = std::max(row_height, figure->size.y);
        figure->prepareInstances(pos + cur_pos, out_instances);
        cur_pos.x += figure->size.x;
      }
      cur_pos.y += row_height;
    }
  }

  int2 Transform::calculateSize(int2 force_size)
  {
    // external force_size has highest priority, but if
    // figure size is explicitly set, it is forced to children
    if (!is_valid_size(force_size) && is_valid_size(size))
      force_size = size;

    int2 figure_size = figure->calculateSize();
    int2 target_size = int2(scale * float2(crop.z - crop.x, crop.w - crop.y) * float2(figure->calculateSize()));

    if (is_valid_size(force_size))
    {
      float2 scale = float2(force_size) / float2(target_size);
      size = int2(std::min(scale.x, scale.y)*float2(target_size)); 
    }
    else
    {
      size = target_size;
    }
    
    if (frame)
      size = frame->calculateSize(size);

    return size;
  }

  bool Transform::load(const Block *blk)
  {
    size = blk->get_ivec2("size", size);
    crop = blk->get_vec4("crop", crop);
    scale = blk->get_vec2("scale", scale);
    rotation = blk->get_double("rotation", rotation);
    mirror_x = blk->get_bool("mirror_x", mirror_x);
    mirror_y = blk->get_bool("mirror_y", mirror_y);

    if (blk->get_block("figure"))
    {
      figure = create_figure(blk->get_block("figure"));
    }
    else
    {
      printf("[Transform::load] transform must contain block with figure\n");
      return false;
    }

    if (blk->get_block("frame"))
    {
      Block fig_frame_blk;
      fig_frame_blk.copy(blk->get_block("frame"));
      fig_frame_blk.set_ivec2("size", int2(1,1)); //real size is the same as image, it will be set later
      frame = std::make_shared<Rectangle>();
      bool frame_loaded = frame->load(&fig_frame_blk);
      if (!frame_loaded)
      {
        printf("[Transform::load] frame failed to load\n");
        return false;
      }
    }

    return true;
  }

  void Transform::prepareInstances(int2 pos, std::vector<Instance> &out_instances)
  {
    auto child_type = figure->getType();
    if (child_type == FigureType::PrimitiveImage || child_type == FigureType::Transform)
    {
      float3x3 crop_trans;
      {
        float x0 = crop.x;
        float y0 = crop.y;
        float x1 = crop.z;
        float y1 = crop.w;
        crop_trans = float3x3(x1 - x0, 0, x0, 0, y1 - y0, y0, 0, 0, 1);
      }
      float3x3 rot = float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1);
      if (rotation != 0)
      {
        float a = rotation * (LiteMath::M_PI / 180.0f);
        float c = cos(a);
        float s = sin(a);
        rot = float3x3(1, 0, 0.5, 0, 1, 0.5, 0, 0, 1) * float3x3(c, -s, 0, s, c, 0, 0, 0, 1) * float3x3(1, 0, -0.5, 0, 1, -0.5, 0, 0, 1);
      }
      float3x3 mirror = float3x3(mirror_x ? -1 : 1, 0, mirror_x ? 1 : 0,
                                 0, mirror_y ? -1 : 1, mirror_y ? 1 : 0,
                                 0, 0, 1);
      std::vector<Instance> instances_to_transform;
      figure->prepareInstances(pos, instances_to_transform);
      float3x3 transform = mirror * rot * crop_trans;
      for (auto &inst : instances_to_transform)
      {
        inst.prim->size = size;
        inst.data.uv_transform = transform * inst.data.uv_transform;
        out_instances.push_back(inst);
      }
    }
    else
    {
      figure->prepareInstances(pos, out_instances);
    }

    if (frame)
    {
      frame->prepareInstances(pos, out_instances);
    }
  }

  std::vector<Instance> prepare_instances(FigurePtr figure)
  {
    int2 actual_size = figure->calculateSize(figure->size);
    printf("actual size %d %d\n", actual_size.x, actual_size.y);
    std::vector<Instance> instances;
    figure->prepareInstances(int2(0, 0), instances);
    return instances;
  }

  FigurePtr create_figure_from_blk(const Block *blk)
  {
    Block temp_blk;
    const Block *figure_blk = nullptr;
    if (blk->get_block("templates") && blk->get_block("figure"))
    {
      const Block *templates_blk = blk->get_block("templates");
      std::map<std::string, const Block *> templates_lib;
      for (int i = 0; i < templates_blk->size(); i++)
      {
        if (templates_blk->get_block(i))
          templates_lib[templates_blk->get_name(i)] = templates_blk->get_block(i);
      }
      temp_blk.copy(blk->get_block("figure"));
      instantiate_all_templates(&temp_blk, templates_lib);
      figure_blk = &temp_blk;

      save_block_to_file("saves/temp_1.blk", temp_blk);
    }
    else if (blk->get_block("figure") && (blk->get_id("type") < 0))
    {
      figure_blk = blk->get_block("figure");
    }
    else
    {
      figure_blk = blk;
    }

    return create_figure(figure_blk);
  }

  LiteImage::Image2D<float4> render_figure_to_image(FigurePtr fig)
  {
    Renderer renderer;
    std::vector<Instance> instances = prepare_instances(fig);
    printf("%d instances, figure size %d %d\n", (int)instances.size(), fig->size.x, fig->size.y);
    LiteImage::Image2D<float4> out = LiteImage::Image2D<float4>(fig->size.x, fig->size.y);

    for (auto &inst : instances)
      renderer.render_instance(inst, out);

    return out;
  }

  void create_and_save_figure(const Block &blk, const std::string &filename)
  {
    FigurePtr fig = create_figure_from_blk(&blk);

    if (fig->getType() == FigureType::Unknown)
    {
      printf("[create_and_save_figure] top level figure type is unknown, probably invalid blk\n");
      return;
    }

    std::string ext = filename.substr(filename.find_last_of(".") + 1);

    if (ext == "bmp" || ext == "png")
    {
      LiteImage::Image2D<float4> out = render_figure_to_image(fig);
      LiteImage::SaveImage(filename.c_str(), out);
    }
    else if (ext == "pdf")
    {
      save_figure_to_pdf(fig, filename);
    }
  }
}