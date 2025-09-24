#include "figure.h"
#include "pdfgen.h"
#include "renderer.h"
#include "stb_image_write.h"
#include "font.h"
#include <string>
#include <vector>

namespace LiteFigure
{
  //Points Per Pixel
  static constexpr int PPP = 4;
  int document_height_points = 0;

  static inline int tonemap(float x, float a_gammaInv) 
  { 
    const int colorLDR = int( std::pow(x, a_gammaInv) * 255.0f + 0.5f );
    if(colorLDR < 0)        return 0;
    else if(colorLDR > 255) return 255;
    else                    return colorLDR;
  }

  static inline float4 tonemap(float4 x, float a_gammaInv)
  {
    return float4(std::pow(x.x, a_gammaInv), std::pow(x.y, a_gammaInv), 
                  std::pow(x.z, a_gammaInv), std::pow(x.w, a_gammaInv));
  }

  static inline uint32_t float4_to_PDF_color(float4 color)
  {
    uint32_t r = std::clamp((uint32_t)(color.x * 255.0f), 0u, 255u);
    uint32_t g = std::clamp((uint32_t)(color.y * 255.0f), 0u, 255u);
    uint32_t b = std::clamp((uint32_t)(color.z * 255.0f), 0u, 255u);
    uint32_t a = 255 - std::clamp((uint32_t)(color.w * 255.0f), 0u, 255u);
    return (a << 24) | (r << 16) | (g << 8) | b;
  }

  static inline uint32_t float4_to_RGBA8_color(float4 color, float a_gammaInv)
  {
    uint32_t r = tonemap(color.x, a_gammaInv);
    uint32_t g = tonemap(color.y, a_gammaInv);
    uint32_t b = tonemap(color.z, a_gammaInv);
    uint32_t a = tonemap(color.w, a_gammaInv);
    return (a << 24) | (b << 16) | (g << 8) | r;
  }

  void float4_image_to_RGBA8_image(LiteImage::Image2D<float4> &src, LiteImage::Image2D<uint32_t> &dst, float gamma = 2.2f)
  {
    dst.resize(src.width(), src.height());
    for (uint32_t y = 0; y < src.height(); y++)
    {
      const uint32_t offset1 = y*src.width();
      const uint32_t offset2 = y*dst.width();
      for(uint32_t x=0; x<src.width(); x++)
      {
        dst.data()[offset2 + x] = float4_to_RGBA8_color(src.data()[offset1 + x], 1.0f/gamma);
      }
    }
  }

  void float4_image_to_RGB8_image(LiteImage::Image2D<float4> &src, std::vector<unsigned char> &dst, float gamma = 2.2f)
  {
    dst.resize(3*src.width()*src.height());
    for (uint32_t y = 0; y < src.height(); y++)
    {
      const uint32_t offset1 = y*src.width();
      const uint32_t offset2 = 3*y*src.width();
      for(uint32_t x=0; x<src.width(); x++)
      {
        float4 color = src.data()[offset1 + x];
        dst.data()[offset2 + x*3 + 0] = tonemap(color.x, 1.0f/gamma);
        dst.data()[offset2 + x*3 + 1] = tonemap(color.y, 1.0f/gamma);
        dst.data()[offset2 + x*3 + 2] = tonemap(color.z, 1.0f/gamma);
      }
    }
  }

  int pdf_add_image_file_flip(struct pdf_doc *pdf, struct pdf_object *page, float x, float y, float w, float h, const char *filename)
  {
    return pdf_add_image_file(pdf, page, x, document_height_points - y - h, w, h, filename);
  }

  int pdf_add_text_flip(struct pdf_doc *pdf, struct pdf_object *page, const char *text, float size, float xoff, float yoff,
                        uint32_t colour)
  {
    return pdf_add_text(pdf, page, text, size, xoff, document_height_points - yoff, colour);
  }

  int pdf_add_line_flip(struct pdf_doc *pdf, struct pdf_object *page, float x1, float y1, float x2, float y2, float width, 
                        uint32_t colour)
  {
    return pdf_add_line(pdf, page, x1, document_height_points - y1, x2, document_height_points - y2, width, colour);
  }

  int pdf_add_filled_rectangle_flip(struct pdf_doc *pdf, struct pdf_object *page, float x, float y, float w, float h, 
                                    uint32_t colour)
  {
    return pdf_add_filled_rectangle(pdf, page, x, document_height_points - y - h, w, h, 0, colour, colour);
  }

  int pdf_add_ellipse_flip(struct pdf_doc *pdf, struct pdf_object *page, float x, float y, float x_radius, float y_radius, 
                           uint32_t colour)
  {
    return pdf_add_ellipse(pdf, page, x, document_height_points - y, x_radius, y_radius, 0, colour, colour);
  }

  bool save_PrimitiveImage_to_pdf(std::shared_ptr<PrimitiveImage> prim, InstanceData inst, struct pdf_doc *pdf)
  {
    static int counter = 0;
    //first, render image (it can be cropped, rotated, etc.)
    LiteImage::Image2D<float4> out = LiteImage::Image2D<float4>(inst.size.x, inst.size.y);
    Renderer renderer;
    Instance image_inst;
    image_inst.prim = prim;
    image_inst.data.pos = int2(0, 0);
    image_inst.data.size = inst.size;
    renderer.render_instance(image_inst, out);

    // then, save to temporary png file. Drop alpha channel
    std::vector<unsigned char> out_RGB8;
    float4_image_to_RGB8_image(out, out_RGB8);
    std::string filename = "tmp/image_" + std::to_string(counter++) + ".png";
    stbi_write_png(filename.c_str(), inst.size.x, inst.size.y, 3, out_RGB8.data(), inst.size.x*3);

    //then add it to pdf
    int res = pdf_add_image_file_flip(pdf, nullptr, PPP*inst.pos.x, PPP*inst.pos.y, 
                                      PPP*inst.size.x, PPP*inst.size.y, filename.c_str());
    if (res < 0)
    {
      fprintf(stderr, "[PDFGen]Error adding image: %d\n", res);
      return false;
    }
    return true;
  }

  bool save_Glyph_to_pdf(std::shared_ptr<Glyph> prim, InstanceData inst, struct pdf_doc *pdf)
  {
    const Font &font = get_font(prim->font_name);
    const TTFSimpleGlyph &glyph = font.glyphs[prim->glyph_id];
    // printf("size %d %d\n", inst.size.x, inst.size.y);
    // printf("font scale %f\n", 1.0f/font.scale);
    // printf("font size %f %f\n", inst.size.x/(font.scale*(glyph.xMax-glyph.xMin)), 
    //                             inst.size.y/(font.scale*(glyph.yMax-glyph.yMin)));
    // printf("glyphs box %d %d %d %d\n", glyph.xMin, glyph.yMin, glyph.xMax, glyph.yMax);
    char ch[2];
    ch[0] = prim->character;
    ch[1] = '\0';
    float sz = PPP*prim->font_size;
    float2 sh = float2(-font.scale*glyph.xMin, font.scale*glyph.yMax);
    int res = pdf_add_text_flip(pdf, nullptr, ch, sz, PPP*inst.pos.x + sh.x*sz, PPP*inst.pos.y + sh.y*sz, 
                                float4_to_PDF_color(prim->color));
    if (res < 0)
    {
      fprintf(stderr, "[PDFGen]Error adding glyph: %d\n", res);
      return false;
    }
    return true;
  }

  bool save_Line_to_pdf(std::shared_ptr<Line> prim, InstanceData inst, struct pdf_doc *pdf)
  {
    int res = pdf_add_line_flip(pdf, nullptr, 
                                PPP*(inst.pos.x + inst.size.x*prim->start.x), PPP*(inst.pos.y + inst.size.y*prim->start.y),
                                PPP*(inst.pos.x + inst.size.x*  prim->end.x), PPP*(inst.pos.y + inst.size.y*  prim->end.y),
                                PPP*prim->thickness*std::max(inst.size.x, inst.size.y),
                                float4_to_PDF_color(tonemap(prim->color, 1.0f/2.2f)));
    if (res < 0)
    {
      fprintf(stderr, "[PDFGen]Error adding line: %d\n", res);
      return false;
    }
    return true;
  }

  bool save_PrimitiveFill_to_pdf(std::shared_ptr<PrimitiveFill> prim, InstanceData inst, struct pdf_doc *pdf)
  {
    int res = pdf_add_filled_rectangle_flip(pdf, nullptr, PPP*inst.pos.x, PPP*inst.pos.y, 
                                            PPP*inst.size.x, PPP*inst.size.y, 
                                            float4_to_PDF_color(tonemap(prim->color, 1.0f/2.2f)));
    if (res < 0)
    {
      fprintf(stderr, "[PDFGen]Error adding rectangle: %d\n", res);
      return false;
    }
    return true;
  }

  bool save_Circle_to_pdf(std::shared_ptr<Circle> prim, InstanceData inst, struct pdf_doc *pdf)
  {
    float center_x = PPP*(inst.pos.x + inst.size.x*prim->center.x);
    float center_y = PPP*(inst.pos.y + inst.size.y*prim->center.y);
    float radius = PPP*prim->radius*std::max(inst.size.x, inst.size.y);
    int res = pdf_add_ellipse_flip(pdf, nullptr, center_x, center_y, radius, radius,
                                   float4_to_PDF_color(tonemap(prim->color, 1.0f/2.2f)));
    if (res < 0)
    {
      fprintf(stderr, "[PDFGen]Error adding circle: %d\n", res);
      return false;
    }
    return true;
  }

  void save_figure_to_pdf(FigurePtr fig, const std::string &filename)
  {
    Renderer renderer;
    std::vector<Instance> instances = prepare_instances(fig);
    printf("%d instances, figure size %d %d\n", (int)instances.size(), fig->size.x, fig->size.y);    

    //TODO: this can be an option too
    struct pdf_info info = {
         .creator = "LiteFugure",
         .producer = "LiteFugure",
         .title = "My document",
         .author = "Author",
         .subject = "Figure",
         .date = "Today"
         };

    struct pdf_doc *pdf = pdf_create(PPP*fig->size.x, PPP*fig->size.y, &info);
    document_height_points = PPP*fig->size.y;
    //our figure is always a single page
    pdf_append_page(pdf);
    pdf_set_font(pdf, "Times-Roman"); //TODO: set font for each text field
    pdf_add_filled_rectangle(pdf, nullptr, 0, 0, PPP*fig->size.x, PPP*fig->size.y, 0, PDF_BLACK, PDF_BLACK);

    for (auto &inst : instances)
    {
      //const auto &inst = instances[i];
      switch (inst.prim->getType())
      {
      case FigureType::PrimitiveImage:
        save_PrimitiveImage_to_pdf(std::dynamic_pointer_cast<PrimitiveImage>(inst.prim), inst.data, pdf);
        break;
      case FigureType::PrimitiveFill:
        save_PrimitiveFill_to_pdf(std::dynamic_pointer_cast<PrimitiveFill>(inst.prim), inst.data, pdf);
        break;
      case FigureType::Glyph:
        save_Glyph_to_pdf(std::dynamic_pointer_cast<Glyph>(inst.prim), inst.data, pdf);
        break;
      case FigureType::Line:
        save_Line_to_pdf(std::dynamic_pointer_cast<Line>(inst.prim), inst.data, pdf);
        break;
      case FigureType::Circle:
        save_Circle_to_pdf(std::dynamic_pointer_cast<Circle>(inst.prim), inst.data, pdf);
        break;
      default:
        printf("[save_figure_to_pdf] Primitive type %d not supported\n", (int)(inst.prim->getType()));
        break;
      }
    }

    pdf_save(pdf, filename.c_str());
    pdf_destroy(pdf);
  }
};