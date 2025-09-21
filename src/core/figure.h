#pragma once
#include <memory>
#include <vector>
#include <string>

#include "LiteMath/Image2d.h"
#include "blk/blk.h"

namespace LiteFigure
{
  enum class FigureType
  {
    Unknown,
    Grid,
    Collage,
    Transform,
    PrimitiveImage,
    PrimitiveFill,
    Line,
    Circle,
    Polygon,
    Text,
    Glyph,
    LinePlot,
    LineGraph
  };
  
  struct Figure
  {
    virtual ~Figure() = default;
    virtual FigureType getType() const = 0;

    // calculates and returns the size of the figure, also sets size variable
    // if force_size is not -1,-1, it will be used to change the size of the 
    // figure and all it's children accordingly. 
    // Even in this case returned size might be different from force_size
    // due to rounding errors or other constaints
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) = 0;

    // loads figure data from blk, returns true on success
    virtual bool load(const Block *blk) = 0;
    int2 size = int2(-1,-1);
    bool verbose = false;
  };
  using FigurePtr = std::shared_ptr<Figure>;

  struct Primitive : public Figure
  {
    virtual FigureType getType() const = 0;
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override
    {
      if (force_size.x > 0 && force_size.y > 0)
        size = force_size;
      return size;
    }
  };

  struct InstanceData
  {
    int2 pos  = int2(0,0);
    int2 size = int2(-1,-1);
    LiteMath::float3x3 uv_transform = LiteMath::float3x3();
  };
  struct Instance
  {
    std::shared_ptr<Primitive> prim;
    InstanceData data;
  };

  struct Grid : public Figure
  {
    virtual FigureType getType() const override { return FigureType::Grid; }
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;

    std::vector<std::vector<FigurePtr>> rows;
  };

  struct Collage : public Figure
  {
    struct Element
    {
      int2 pos  = int2(0,0);
      int2 size = int2(-1,-1);
      FigurePtr figure;
    };

    virtual FigureType getType() const override { return FigureType::Collage; }
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;

    std::vector<Element> elements;
  };

  struct Transform : public Figure
  {
    virtual FigureType getType() const override { return FigureType::Transform; }
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;

    FigurePtr figure;
    float4 crop  = float4(0,0,1,1);
    float2 scale = float2(1,1);
    float rotation = 0; // in degrees
    bool mirror_x = false;
    bool mirror_y = false;
  };

  struct PrimitiveImage : public Primitive
  {
    virtual FigureType getType() const override { return FigureType::PrimitiveImage; }
    virtual bool load(const Block *blk) override;

    LiteImage::Sampler sampler;
    LiteImage::Image2D<float4> image;
  };

  struct PrimitiveFill : public Primitive
  {
    virtual FigureType getType() const override { return FigureType::PrimitiveFill; }
    virtual bool load(const Block *blk) override;

    float4 color = float4(1,0,0,1);
  };

  enum class LineStyle
  {
    Solid,
    Dashed,
    Dotted
  };

  struct Line : public Primitive
  {
    virtual FigureType getType() const override { return FigureType::Line; }
    virtual bool load(const Block *blk) override;

    LineStyle style = LineStyle::Solid;
    float4 color = float4(0,0,0,1);
    // all in normalized coordinates (0..1)
    float2 start = float2(0,0);
    float2 end = float2(1,1);
    float2 style_pattern = float2(1,0); // dash length/dot radius, space length
    float thickness = 0.01f;
    bool antialiased = true;
  };

  struct Circle : public Primitive
  {
    virtual FigureType getType() const override { return FigureType::Circle; }
    virtual bool load(const Block *blk) override;

    float4 color = float4(0,0,0,1);
    // center and radius are in normalized coordinates (0..1)
    float2 center = float2(0.5f,0.5f);
    float radius = 0.25f;
    bool antialiased = true;
  };

  struct Polygon : public Primitive
  {
    virtual FigureType getType() const override { return FigureType::Polygon; }
    virtual bool load(const Block *blk) override;

    struct Contour
    {
      std::vector<float2> points; // in normalized coordinates (0..1)
    };

    float4 color = float4(0,0,0,1);
    bool outline = false;
    float outline_thickness = 0.01f; // in normalized coordinates (0..1)
    bool  outline_antialiased = true;
    std::vector<Contour> contours;
  };

  struct Glyph : public Primitive
  {
    virtual FigureType getType() const override { return FigureType::Glyph; }
    virtual bool load(const Block *blk) override;

    char character = '\0';
    int glyph_id = 0;
    int font_size = 64;
    std::string font_name;
    float4 color = float4(0,0,0,1);
  };

	enum class TextAlignmentX
	{
		Left,
		Right,
		Center,
	};

	enum class TextAlignmentY
	{
		Top,
		Bottom,
		Center,
	};

  struct Text : public Figure
  {
    virtual FigureType getType() const override { return FigureType::Text; }
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;

    void prepare_glyphs(int2 pos, std::vector<Instance> &out_instances);

    int font_size = 64;
    std::string text;
    std::string font_name = "fonts/times-new-roman-regular.ttf";
    bool retain_width = false;
    bool retain_height = false;
    float4 color = float4(1,1,1,1);
    float4 background_color = float4(0,0,0,0);
    TextAlignmentX alignment_x = TextAlignmentX::Left;
    TextAlignmentY alignment_y = TextAlignmentY::Top;
  private:
    std::vector<int2> glyph_positions;
    std::vector<Glyph> glyphs;
  };

  struct LineGraph : public Figure
  {
    virtual FigureType getType() const override { return FigureType::LineGraph; }
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;
    void prepare_instances(int2 pos, std::vector<Instance> &out_instances);

    float4 color = float4(1,0,0,1);
    float thickness = 0.0075f;
    bool use_points = true;
    float point_size = 0.01f;
    std::vector<float2> values; // in normalized coordinates (0..1)
  private:
    std::vector<Line> lines;
    std::vector<Circle> points;
  };

  struct LinePlot : public Figure
  {
    virtual FigureType getType() const override { return FigureType::LinePlot; }
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;
    void prepare_instances(int2 pos, std::vector<Instance> &out_instances);

  private:
    Text header;
    PrimitiveFill background;

    Text x_label;
    Line x_axis;
    std::vector<Text> x_ticks;
    std::vector<Line> x_tick_lines;

    Text y_label;
    Line y_axis;
    std::vector<Text> y_ticks;
    std::vector<Line> y_tick_lines;

    std::vector<LineGraph> graphs;

    std::shared_ptr<Collage> full_graph_collage;
    //std::vector<Text> values;
    //PrimitiveFill legend_box;
    //std::vector<Text> legend_labels;
    //std::vector<Line> legend_lines;
  };

  static bool is_valid_size(int2 size) { return size.x > 0 && size.y > 0; }
  static bool equal(int2 a, int2 b) { return a.x == b.x && a.y == b.y; }

  FigurePtr create_figure_from_blk(const Block *blk);
  LiteImage::Image2D<float4> render_figure_to_image(FigurePtr figure);
  void create_and_save_figure(const Block &blk, const std::string &filename);
  std::vector<Instance> prepare_instances(FigurePtr figure);
  void save_figure_to_pdf(FigurePtr fig, const std::string &filename);
}