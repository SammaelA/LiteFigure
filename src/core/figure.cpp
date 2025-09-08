#include "figure.h"
#include <cstdio>

namespace LiteFigure
{
  REGISTER_ENUM(FigureType,
              ([]()
               { return std::vector<std::pair<std::string, unsigned>>{
                     {"Unknown", (unsigned)FigureType::Unknown},
                     {"Grid", (unsigned)FigureType::Grid},
                     {"Collage", (unsigned)FigureType::Collage},
                     {"TemplateInstance", (unsigned)FigureType::TemplateInstance},
                     {"Transform", (unsigned)FigureType::Transform},
                     {"PrimitiveImage", (unsigned)FigureType::PrimitiveImage},
                     {"PrimitiveFill", (unsigned)FigureType::PrimitiveFill},
                 }; })());
  
  int2 get_invalid_size() { return int2(-1,-1); }
  bool is_valid_size(int2 size) { return size.x > 0 && size.y > 0; }
  bool equal(int2 a, int2 b) { return a.x == b.x && a.y == b.y; }

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
  

  
  int2 Grid::calculateSize(int2 force_size)
  {
    // external force_size has highest priority, but if 
    // figure size is explicitly set, it is forced to children
    if (!is_valid_size(force_size) && is_valid_size(size))
      force_size = size;

    int2 min_pos(0, 0); //always start from 0 for grid
    int2 max_pos(-1, -1);
    int2 cur_pos = int2(0,0);
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
    cur_pos = int2(0,0);
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

  int2 Transform::calculateSize(int2 force_size)
  {
    // external force_size has highest priority, but if 
    // figure size is explicitly set, it is forced to children
    if (!is_valid_size(force_size) && is_valid_size(size))
      force_size = size;

    if (!is_valid_size(force_size))
      force_size = int2(scale * float2(crop.z-crop.x, crop.w-crop.y) * float2(figure->calculateSize()));

    size = figure->calculateSize(force_size);
    return size;
  }

  void prepare_instances_rec(FigurePtr figure, int2 pos, std::vector<Instance> &instances)
  {
    if (dynamic_cast<Primitive*>(figure.get()) && is_valid_size(figure->size))
    {
      Instance inst;
      inst.prim = std::dynamic_pointer_cast<Primitive>(figure);
      inst.pos = pos;
      inst.size = figure->size;
      instances.push_back(inst);
    }
    else if (dynamic_cast<Collage*>(figure.get()))
    {
      Collage *collage = dynamic_cast<Collage*>(figure.get());
      for (int i = 0; i < collage->elements.size(); i++)
      {
        prepare_instances_rec(collage->elements[i].figure, pos + collage->elements[i].pos, instances);
      }
    }
    else if (dynamic_cast<Grid*>(figure.get()))
    {
      Grid *grid = dynamic_cast<Grid*>(figure.get());
      int2 cur_pos = int2(0,0); //local position in grid
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
    else if (dynamic_cast<Transform*>(figure.get()))
    {
      Transform *transform = dynamic_cast<Transform*>(figure.get());

      auto child_type = transform->figure->getType();
      if (child_type == FigureType::PrimitiveImage || child_type == FigureType::Transform)
      {
        float3x3 crop_trans;
        {
          float x0 = transform->crop.x;
          float y0 = transform->crop.y;
          float x1 = transform->crop.z;
          float y1 = transform->crop.w;
          crop_trans = float3x3(x1-x0, 0, x0,  0, y1-y0, y0, 0, 0, 1);
        }
        std::vector<Instance> instances_to_transform;
        prepare_instances_rec(transform->figure, pos, instances_to_transform);
        for (auto &inst : instances_to_transform)
        {
          inst.uv_transform = crop_trans * inst.uv_transform;
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
    prepare_instances_rec(figure, int2(0,0), instances);
    return instances;
  }

  static inline float4 alpha_blend(float4 a, float4 b)
  {
    //printf("sample %f %f %f %f and %f %f %f %f\n", a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w);
    float4 r;
    r.w = a.w + b.w*(1.0f-a.w) + 1e-9f;
    r.x = (a.x*a.w + b.x*b.w*(1.0f-a.w))/r.w;
    r.y = (a.y*a.w + b.y*b.w*(1.0f-a.w))/r.w;
    r.z = (a.z*a.w + b.z*b.w*(1.0f-a.w))/r.w; 
    return r;
  }

  void render_primitive_image(const PrimitiveImage &prim, int2 pos, int2 size, const LiteMath::float3x3 &uv_transform, 
                              LiteImage::Image2D<float4> &out)
  {
    for (int y = 0; y < size.y; y++)
    {
      for (int x = 0; x < size.x; x++)
      {
        float3 uv = uv_transform * float3(x/float(size.x), y/float(size.y), 1);
        float4 c = prim.image.sample(prim.sampler, float2(uv.x, uv.y));
        out[uint2(x+pos.x, y+pos.y)] = alpha_blend(c, out[uint2(x+pos.x, y+pos.y)]);
      }
    }
  }

  void render_primitive_fill(const PrimitiveFill &prim, int2 pos, int2 size, 
                             LiteImage::Image2D<float4> &out)
  {
    float4 c = prim.color;
    for (int y = pos.y; y < pos.y+size.y; y++)
    {
      for (int x = pos.x; x < pos.x+size.x; x++)
      {
        out[uint2(x, y)] = alpha_blend(c, out[uint2(x, y)]);
      }
    }
  }

  void render_instance(const Instance &inst, LiteImage::Image2D<float4> &out)
  {
    switch (inst.prim->getType())
    {
      case FigureType::PrimitiveImage:
        render_primitive_image(*dynamic_cast<PrimitiveImage*>(inst.prim.get()), inst.pos, inst.size, 
                                                              inst.uv_transform, out);
        break;
      case FigureType::PrimitiveFill:
        render_primitive_fill(*dynamic_cast<PrimitiveFill*>(inst.prim.get()), inst.pos, inst.size, out);
        break;
      default:
        printf("[render_instance] unsupported primitive type %d\n", (int)(inst.prim->getType()));
        break;
    }
  }

  FigurePtr create_figure(const Block *blk);

  FigurePtr create_error_figure_dummy()
  {
    std::shared_ptr<PrimitiveFill> prim = std::make_shared<PrimitiveFill>();
    prim->size = int2(64, 64);
    prim->color = float4(1,0,1,1);
    return prim;
  }

  FigurePtr create_figure_primitive_fill(const Block *blk)
  {
    std::shared_ptr<PrimitiveFill> prim = std::make_shared<PrimitiveFill>();
    prim->size = blk->get_ivec2("size", prim->size);
    prim->color = blk->get_vec4("color", prim->color);

    if (prim->size.x < 1 || prim->size.y < 1)
    {
      printf("[create_figure_primitive_fill] size must be explicitly declared\n");
      return create_error_figure_dummy();
    }

    return prim;
  }

  FigurePtr create_figure_primitive_image(const Block *blk)
  {
    std::shared_ptr<PrimitiveImage> prim = std::make_shared<PrimitiveImage>();
    prim->size = blk->get_ivec2("size", prim->size);
    std::string path = blk->get_string("path", "");
    if (path == "")
    {
      printf("[create_figure_primitive_image] path is empty\n");
      return create_error_figure_dummy();
    }
    prim->image = LiteImage::LoadImage<float4>(path.c_str());
    //TODO: set sampler
    //TODO: support images with alpha
    for (int i=0;i<prim->image.height()*prim->image.width();i++)
      prim->image.data()[i].w = 1.0f;

    if (prim->image.width() < 1 || prim->image.height() < 1)
    {
      printf("[create_figure_primitive_image] image is invalid\n");
      return create_error_figure_dummy();
    }
    else if (prim->size.x < 1 || prim->size.y < 1)
    {
      prim->size = int2(prim->image.width(), prim->image.height());
    }

    return prim;
  }

  FigurePtr create_figure_collage(const Block *blk)
  {
    std::shared_ptr<Collage> collage = std::make_shared<Collage>();
    collage->size = blk->get_ivec2("size", collage->size);
    for (int i = 0; i < blk->size(); i++)
    {
      Block *elem_blk = blk->get_block(i);
      if (elem_blk)
      {
        Collage::Element elem;
        elem.pos = elem_blk->get_ivec2("pos", elem.pos);
        elem.size = elem_blk->get_ivec2("size", elem.size);
        elem.figure = create_figure(elem_blk);
        collage->elements.push_back(elem);
      }
    }

    if (collage->elements.size() == 0)
    {
      printf("[create_figure_collage] collage is empty\n");
      return create_error_figure_dummy();
    }

    return collage;
  } 

  FigurePtr create_figure_grid(const Block *blk)
  {
    std::shared_ptr<Grid> grid = std::make_shared<Grid>();
    grid->size = blk->get_ivec2("size", grid->size);
    for (int i = 0; i < blk->size(); i++)
    {
      Block *row_blk = blk->get_block(i);
      if (!row_blk)
        continue;

      grid->rows.emplace_back();

      for (int j = 0; j < row_blk->size(); j++)
      {
        Block *elem_blk = row_blk->get_block(j);
        if (elem_blk)
          grid->rows.back().push_back(create_figure(elem_blk));
      }

      if (grid->rows.back().size() == 0)
      {
        printf("[create_figure_grid] row is empty\n");
        return create_error_figure_dummy();
      }
    }

    if (grid->rows.size() == 0)
    {
      printf("[create_figure_grid] grid is empty\n");
      return create_error_figure_dummy();
    }

    return grid;
  }

  FigurePtr create_figure_transform(const Block *blk)
  {
    std::shared_ptr<Transform> transform = std::make_shared<Transform>();
    transform->size = blk->get_ivec2("size", transform->size);
    transform->crop = blk->get_vec4("crop", transform->crop);
    transform->scale = blk->get_vec2("scale", transform->scale);

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
          printf("[create_figure_transform] only one figure block allowed for transform\n");
          return create_error_figure_dummy();
        }
      }
    }

    if (figure_block_id == -1)
    {
      printf("[create_figure_transform] transform must contain block with figure\n");
      return create_error_figure_dummy(); 
    }
    else
    {
      transform->figure = create_figure(blk->get_block(figure_block_id));
    }

    return transform;
  }

  FigurePtr create_figure(const Block *blk)
  {
    switch ((FigureType)blk->get_enum("type", (unsigned)FigureType::Unknown))
    {
      case FigureType::Unknown:
        printf("[create_figure] unknown figure type\n");
        return create_error_figure_dummy();
      case FigureType::Grid:
        return create_figure_grid(blk);
      case FigureType::Collage:
        return create_figure_collage(blk);
      case FigureType::TemplateInstance:
        printf("[create_figure] unsupported figure type TemplateInstance\n");
        return create_error_figure_dummy();
      case FigureType::Transform:
        return create_figure_transform(blk);
      case FigureType::PrimitiveImage:
        return create_figure_primitive_image(blk);
      case FigureType::PrimitiveFill:
        return create_figure_primitive_fill(blk);
      default:
        printf("[create_figure] unsupported figure type %d\n", (int)blk->get_enum("type", (unsigned)FigureType::Unknown));
        return create_error_figure_dummy();
    }

    return nullptr;
  }

  void create_and_save_figure(const Block &blk, const std::string &filename)
  {
    FigurePtr fig = create_figure(&blk);

    if (fig->getType() == FigureType::Unknown)
    {
      printf("[create_and_save_figure] top level figure type is unknown, probably invalid blk\n");
      return;
    }

    std::vector<Instance> instances = prepare_instances(fig);
    LiteImage::Image2D<float4> out = LiteImage::Image2D<float4>(fig->size.x, fig->size.y);
    for (auto &inst : instances)
    {
      printf("render %d %d\n", inst.pos.x, inst.pos.y);
      render_instance(inst, out);
    }
    LiteImage::SaveImage(filename.c_str(), out);
  }
}