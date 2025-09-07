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


  void prepare_instances_rec(FigurePtr figure, int2 pos, std::vector<Instance> &instances)
  {
    if (dynamic_cast<Primitive*>(figure.get()) && figure->size.x > 0 && figure->size.y > 0)
    {
      Instance inst;
      inst.prim = std::dynamic_pointer_cast<Primitive>(figure);
      inst.pos = pos;
      inst.size = figure->size;
      instances.push_back(inst);
    }
  }

  std::vector<Instance> prepare_instances(FigurePtr figure)
  {
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

  void render_primitive_image(const PrimitiveImage &prim, int2 pos, int2 size, 
                              LiteImage::Image2D<float4> &out)
  {
    for (int y = 0; y < size.y; y++)
    {
      for (int x = 0; x < size.x; x++)
      {
        float2 uv = float2(x/float(size.x), y/float(size.y));
        float4 c = prim.image.sample(prim.sampler, uv);
        out[uint2(x+pos.x, y+pos.y)] = alpha_blend(out[uint2(x+pos.x, y+pos.y)], c);
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
        out[uint2(x, y)] = alpha_blend(out[uint2(x, y)], c);
      }
    }
  }

  void render_instance(const Instance &inst, LiteImage::Image2D<float4> &out)
  {
    switch (inst.prim->getType())
    {
      case FigureType::PrimitiveImage:
        render_primitive_image(*dynamic_cast<PrimitiveImage*>(inst.prim.get()), inst.pos, inst.size, out);
        break;
      case FigureType::PrimitiveFill:
        render_primitive_fill(*dynamic_cast<PrimitiveFill*>(inst.prim.get()), inst.pos, inst.size, out);
        break;
      default:
        printf("[render_instance] unsupported primitive type %d\n", (int)(inst.prim->getType()));
        break;
    }
  }

  FigurePtr create_error_figure_dummy()
  {
    std::shared_ptr<PrimitiveFill> prim = std::make_shared<PrimitiveFill>();
    prim->size = int2(-1,-1);
    prim->color = float4(1,0,1,1);
    return prim;
  }

  FigurePtr create_figure_primitive_fill(const Block &blk)
  {
    std::shared_ptr<PrimitiveFill> prim = std::make_shared<PrimitiveFill>();
    prim->size = blk.get_ivec2("size", prim->size);
    prim->color = blk.get_vec4("color", prim->color);
    return prim;
  }

  FigurePtr create_figure_primitive_image(const Block &blk)
  {
    std::shared_ptr<PrimitiveImage> prim = std::make_shared<PrimitiveImage>();
    prim->size = blk.get_ivec2("size", prim->size);
    std::string path = blk.get_string("path", "");
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
    return prim;
  }

  FigurePtr create_figure(const Block &blk)
  {
    switch ((FigureType)blk.get_enum("type", (unsigned)FigureType::Unknown))
    {
      case FigureType::Unknown:
        printf("[create_figure] unknown figure type\n");
        return create_error_figure_dummy();
      case FigureType::Grid:
        printf("[create_figure] unsupported figure type Grid\n");
        return create_error_figure_dummy();
      case FigureType::Collage:
        printf("[create_figure] unsupported figure type Collage\n");
        return create_error_figure_dummy();
      case FigureType::TemplateInstance:
        printf("[create_figure] unsupported figure type TemplateInstance\n");
        return create_error_figure_dummy();
      case FigureType::Transform:
        printf("[create_figure] unsupported figure type Transform\n");
        return create_error_figure_dummy();
      case FigureType::PrimitiveImage:
        return create_figure_primitive_image(blk);
      case FigureType::PrimitiveFill:
        return create_figure_primitive_fill(blk);
      default:
        printf("[create_figure] unsupported figure type %d\n", (int)blk.get_enum("type", (unsigned)FigureType::Unknown));
        return create_error_figure_dummy();
    }

    return nullptr;
  }

  void create_and_save_figure(const Block &blk, const std::string &filename)
  {
    FigurePtr fig = create_figure(blk);

    if (fig->getType() == FigureType::Unknown)
    {
      printf("[create_and_save_figure] top level figure type is unknown, probably invalid blk\n");
      return;
    }

    if (fig->size.x < 1 || fig->size.y < 1)
    {
      printf("[create_and_save_figure] top level figure must have determined size\n");
      return;
    }

    LiteImage::Image2D<float4> out = LiteImage::Image2D<float4>(fig->size.x, fig->size.y);
    std::vector<Instance> instances = prepare_instances(fig);
    for (auto &inst : instances)
    {
      render_instance(inst, out);
    }
    LiteImage::SaveImage(filename.c_str(), out);
  }
}