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
    LineGraph,
    Rectangle
  };
  
  struct Instance;
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

    // recursively prepares a set to instances (primitives + positions) to 
    // render or somehow display this figure
    virtual void prepareInstances(int2 pos, std::vector<Instance> &out_instances) = 0;

    // loads figure data from blk, returns true on success
    virtual bool load(const Block *blk) = 0;
    int2 size = int2(-1,-1);
    bool verbose = false;
  };
  using FigurePtr = std::shared_ptr<Figure>;

  struct Primitive : public Figure
  {
    virtual FigureType getType() const = 0;
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual void prepareInstances(int2 pos, std::vector<Instance> &out_instances) override;
  };

  struct InstanceData
  {
    int2 pos  = int2(0,0);
    int2 size = int2(-1,-1);
    LiteMath::float3x3 uv_transform = LiteMath::float3x3();
  };
  struct Instance
  {
    Primitive *prim; //pointer to figure tree, no ownership
    InstanceData data;
  };

  struct Grid : public Figure
  {
    virtual FigureType getType() const override { return FigureType::Grid; }
    virtual void prepareInstances(int2 pos, std::vector<Instance> &out_instances) override;
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;

    std::vector<std::vector<FigurePtr>> rows;
  };

  struct Collage : public Figure
  {
    struct Element
    {
      Element() = default;
      Element(int2 _pos, int2 _size, FigurePtr _figure) : pos(_pos), size(_size), figure(_figure) {}
      int2 pos  = int2(0,0);
      int2 size = int2(-1,-1);
      FigurePtr figure;
    };

    virtual FigureType getType() const override { return FigureType::Collage; }
    virtual void prepareInstances(int2 pos, std::vector<Instance> &out_instances) override;
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;

    std::vector<Element> elements;
  };

  struct Rectangle;
  struct Transform : public Figure
  {
    virtual FigureType getType() const override { return FigureType::Transform; }
    virtual void prepareInstances(int2 pos, std::vector<Instance> &out_instances) override;
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;

    FigurePtr figure;
    std::shared_ptr<Rectangle> frame;
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

  struct Rectangle : public Primitive
  {
    virtual FigureType getType() const override { return FigureType::Rectangle; }
    virtual bool load(const Block *blk) override;

    float4 color = float4(1,0,0,1);
    float4 region = float4(0,0,1,1); // (left, top, right, bottom) coordinates of an actual rectangle. Useful for cropping
    float thickness = 0.01f; //in normalized coordinates (0..1), 0.5f will fill the whole frame body
    int thickness_pixel = 0; //fixed thickness in pixels, used instead of "float thickness" if set
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
    float thickness = 0.01f; // in normalized coordinates (0..1)
    int thickness_pixel = 0; // fixed thickness in pixels, used instead of "float thickness" if set
  
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
    virtual void prepareInstances(int2 pos, std::vector<Instance> &out_instances) override;
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;

    int font_size = 64;
    std::string text;
    std::string font_name = "Times-Roman";
    bool retain_width = false;
    bool retain_height = false;
    float4 color = float4(1,1,1,1);
    float4 background_color = float4(0,0,0,0);
    TextAlignmentX alignment_x = TextAlignmentX::Left;
    TextAlignmentY alignment_y = TextAlignmentY::Top;
  private:
    int2 placeGlyphs();
    std::vector<int2> glyph_positions;
    std::vector<Glyph> glyphs;
    PrimitiveFill background_fill;
  };

  struct LinePlot;
  struct LineGraph : public Figure
  {
    friend struct LinePlot; 
    virtual FigureType getType() const override { return FigureType::LineGraph; }
    virtual void prepareInstances(int2 pos, std::vector<Instance> &out_instances) override;
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;

    float4 color = float4(1,0,0,1);
    float thickness = 0.005f;
    bool use_points = true;
    float point_size = 0.0067f;
    std::string name = "unnamed graph";
    std::vector<float2> values; // in normalized coordinates (0..1)
    std::vector<std::string> labels_str;
    bool labels_from_y_values = false;
  private:
    bool load_line_params(const Block *blk);
    bool load_text_params(const Block *blk);
    void rebuid();

    Text base_text;
    std::shared_ptr<Collage> line_graph_collage;
  };

  enum class YLabelPosition
  {
    None,
    Left,
    Top
  };

  enum class LegendPosition
  {
    None,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    InsideGraph
  };

  enum class ColorPalette
  {
    Gray10, //10 shades of gray
    Set1, //Set1 colormap from Matplotlib
    Set2, //Set2 colormap from Matplotlib
  };

  struct LinePlot : public Figure
  {
    virtual FigureType getType() const override { return FigureType::LinePlot; }
    virtual void prepareInstances(int2 pos, std::vector<Instance> &out_instances) override;
    virtual int2 calculateSize(int2 force_size = int2(-1,-1)) override;
    virtual bool load(const Block *blk) override;

  private:
    std::shared_ptr<Collage> create_legend_collage(const Block *blk, const Text &default_text,
                                                   const Line &default_line, int2 full_size);
    bool load_graphs_block(const Block *blk, const Text &default_text,
                           const Line &default_line, int2 full_size);

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
  void create_and_save_multiple_figures(const Block &blk);
  void create_and_save_figure(const Block &blk, const std::string &filename);
  std::vector<Instance> prepare_instances(FigurePtr figure);
  void save_figure_to_pdf(FigurePtr fig, const std::string &filename);
}