#include "figure.h"
#include "templates.h"
#include "renderer.h"
#include "font.h"
#include <cstdio>

namespace LiteFigure
{
  bool LineGraph::load(const Block *blk)
  {
    size = blk->get_ivec2("size", size);
    color = blk->get_vec4("color", color);
    thickness = blk->get_double("thickness", thickness);
    use_points = blk->get_bool("use_points", use_points);
    point_size = blk->get_double("point_size", point_size);
    
    std::vector<float> x_values, y_values;
    blk->get_arr("x_values", x_values);
    blk->get_arr("y_values", y_values);

    if (x_values.size() != y_values.size())
    {
      printf("[LineGraph::load] x_values and y_values must have the same size\n");
      return false;
    }

    for (int i = 0; i < x_values.size(); i++)
    {
      values.emplace_back(x_values[i], y_values[i]);
    }

    return true;
  }

  int2 LineGraph::calculateSize(int2 force_size)
  {
    if (is_valid_size(force_size))
      size = force_size;
    return size;
  }

  void LineGraph::prepare_instances(int2 pos, std::vector<Instance> &out_instances)
  {
    //TODO: handle invalid values (<0 or >1)
    lines.resize(values.size() - 1);
    for (int i=0;i<lines.size();i++)
    {
      lines[i].size = size;
      lines[i].start = values[i];
      lines[i].end = values[i + 1];
      lines[i].thickness = thickness;
      lines[i].color = color;
    }

    if (use_points)
    {
      points.clear();
      points.resize(values.size());
      for (int i=0;i<points.size();i++)
      {
        points[i].size = size;
        points[i].center = values[i];
        points[i].radius = point_size;
        points[i].color = color;
      }
    }

    for (auto &line : lines)
    {
      Instance inst;
      inst.prim = std::make_shared<Line>(line);
      inst.data.pos = pos;
      inst.data.size = size;
      out_instances.push_back(inst);
    }

    for (auto &point : points)
    {
      Instance inst;
      inst.prim = std::make_shared<Circle>(point);
      inst.data.pos = pos;
      inst.data.size = size;
      out_instances.push_back(inst);
    }
  }

  bool LinePlot::load(const Block *blk)
  {
    Block dummy_block;

    float4 background_color = float4(1,1,1,1);
    float2 plot_proportions = float2(1,1);

    size = blk->get_ivec2("size", size);
    background_color = blk->get_vec4("background_color", background_color);
    plot_proportions = blk->get_vec2("plot_proportions", plot_proportions);
    plot_proportions = float2(1, plot_proportions.y/plot_proportions.x);

    Text default_text;
    default_text.alignment_x = TextAlignmentX::Center;
    default_text.alignment_y = TextAlignmentY::Center;
    default_text.background_color = background_color;
    default_text.color = blk->get_vec4("text_color", float4(0,0,0,1));
    default_text.font_name = blk->get_string("font_name", "fonts/times-new-roman-regular.ttf");
    default_text.font_size = blk->get_int("font_size", 64);
    default_text.retain_height = false;
    default_text.retain_width = false;
    default_text.size = int2(-1,-1);
    default_text.text = "???";

    Line default_line;
    default_line.size = size;
    default_line.antialiased = true;
    default_line.color = float4(0.25,0.25,0.25,1);
    default_line.style = LineStyle::Solid;
    default_line.thickness = 0.0033f;

    background.color = background_color;

    header = default_text;
    header.retain_width = true;
    header.size = int2(size.x, -1);
    if (blk->get_block("header"))
    {
      header.load(blk->get_block("header"));
    }


    float2 x_range = float2(1e38f, -1e38f);
    float2 y_range = float2(1e38f, -1e38f);
    for (int i=0;i<blk->size();i++)
    {
      if (blk->get_name(i) != "graph" || !blk->get_block(i))
        continue;

      const Block *graph_blk = blk->get_block(i);
      std::string name = graph_blk->get_string("name", "graph_" + std::to_string(graphs.size()));

      std::vector<float> x_values, y_values;
      graph_blk->get_arr("x_values", x_values);
      graph_blk->get_arr("y_values", y_values);
      if (x_values.size() != y_values.size())
      {
        printf("[LinePlot::load] x_values and y_values must be the same size\n");
        return false;
      }
      if (x_values.size() < 2)
      {
        printf("[LinePlot::load] x_values and y_values must have at least 2 points\n");
        return false;
      }
      LineGraph graph;
      graph.values.resize(x_values.size());
      for (int j = 0; j < x_values.size(); j++)
      {
        graph.values[j] = float2(x_values[j], y_values[j]);
        x_range = min(x_range, float2(x_values[j], x_values[j]));
        y_range = min(y_range, float2(y_values[j], y_values[j]));
      }
      if (graph_blk->get_block("line"))
      {
        graph.load(graph_blk->get_block("line"));
      }
      graphs.push_back(graph);
    }

    x_range = blk->get_vec2("x_range", x_range);
    y_range = blk->get_vec2("y_range", y_range); 

    //rescale values
    for (auto &graph : graphs)
    {
      for (auto &val : graph.values)
      {
        val.x = (val.x - x_range.x) / (x_range.y - x_range.x);
        val.y = 1.0f - (val.y - y_range.x) / (y_range.y - y_range.x);
      }
    }

    x_axis = default_line;
    x_axis.start = float2(0,1);
    x_axis.end = float2(1,1);
    if (blk->get_block("x_axis"))
    {
      x_axis.load(blk->get_block("x_axis"));
    }

    x_label = default_text;
    x_label.retain_width = true;
    x_label.size = int2(size.x, -1);
    if (blk->get_block("x_label"))
    {
      x_label.load(blk->get_block("x_label"));
    }

    std::vector<float> x_tick_values;
    int tick_count = 5;
    if (blk->get_block("x_ticks"))
    {
      const Block *x_ticks_blk = blk->get_block("x_ticks");
      x_ticks_blk->get_arr("values", x_tick_values);
      tick_count = x_ticks_blk->get_int("count", tick_count);
    }
    if (x_tick_values.empty())
    {
      float step = (x_range.y - x_range.x) / float(tick_count);
      for (int i = 0; i < tick_count; i++)
      {
        x_tick_values.push_back(x_range.x + step * (i+0.5f));
      }
    }
    printf("created %d x tick values\n", (int)x_tick_values.size());
    x_tick_lines.resize(x_tick_values.size());
    for (int i = 0; i < x_tick_values.size(); i++)
    {
      x_tick_lines[i] = default_line;
      if (blk->get_block("x_tick_lines"))
      {
        x_tick_lines[i].load(blk->get_block("x_tick_lines"));
      }
      x_tick_lines[i].start = float2((x_tick_values[i] - x_range.x) / (x_range.y - x_range.x), 1);
      x_tick_lines[i].end = float2((x_tick_values[i] - x_range.x) / (x_range.y - x_range.x), 0);
    }

    y_axis = default_line;
    y_axis.start = float2(0,1);
    y_axis.end = float2(0,0);
    if (blk->get_block("y_axis"))
    {
      y_axis.load(blk->get_block("y_axis"));
    }

    y_label = default_text;
    y_label.retain_height = true;
    y_label.size = int2(1, size.y);
    if (blk->get_block("y_label"))
    {
      y_label.load(blk->get_block("y_label"));
    }

    std::vector<float> y_tick_values;
    tick_count = 5;
    if (blk->get_block("y_ticks"))
    {
      const Block *y_ticks_blk = blk->get_block("y_ticks");
      y_ticks_blk->get_arr("values", y_tick_values);
      tick_count = y_ticks_blk->get_int("count", tick_count);
    }
    if (y_tick_values.empty())
    {
      float step = (y_range.y - y_range.x) / float(tick_count);
      for (int i = 0; i < tick_count; i++)
      {
        y_tick_values.push_back(y_range.x + step * (i+0.5f));
      }
    }
    printf("created %d y tick values\n", (int)y_tick_values.size());
    y_tick_lines.resize(y_tick_values.size());
    for (int i = 0; i < y_tick_values.size(); i++)
    {
      y_tick_lines[i] = default_line;
      printf("line size %d %d\n", y_tick_lines[i].size.x, y_tick_lines[i].size.y);
      if (blk->get_block("y_tick_lines"))
      {
        y_tick_lines[i].load(blk->get_block("y_tick_lines"));
      }
      y_tick_lines[i].start = float2(0, 1-(y_tick_values[i] - y_range.x) / (y_range.y - y_range.x));
      y_tick_lines[i].end = float2(1, 1-(y_tick_values[i] - y_range.x) / (y_range.y - y_range.x));
    }

    std::shared_ptr<Collage> body = std::make_shared<Collage>();
    body->size = int2(size.x, plot_proportions.y * size.x);
    for (auto &line : x_tick_lines)
    {
      Collage::Element elem;
      elem.pos = int2(0,0);
      elem.size = int2(size.x, size.y);
      elem.figure = std::make_shared<Line>(line);
      body->elements.push_back(elem);
    }
    for (auto &line : y_tick_lines)
    {
      Collage::Element elem;
      elem.pos = int2(0,0);
      elem.size = int2(size.x, size.y);
      elem.figure = std::make_shared<Line>(line);
      body->elements.push_back(elem);
    }
    for (auto &graph : graphs)
    {
      Collage::Element elem;
      elem.pos = int2(0,0);
      elem.size = int2(size.x, size.y);
      elem.figure = std::make_shared<LineGraph>(graph);
      body->elements.push_back(elem);
    }
    {
      Collage::Element elem;
      elem.pos = int2(0,0);
      elem.size = int2(size.x, size.y);
      elem.figure = std::make_shared<Line>(x_axis);
      body->elements.push_back(elem);
    }
    {
      Collage::Element elem;
      elem.pos = int2(0,0);
      elem.size = int2(size.x, size.y);
      elem.figure = std::make_shared<Line>(y_axis);
      body->elements.push_back(elem);
    }
    
    graph_grid = std::make_shared<Grid>();
    graph_grid->rows.resize(2);
    graph_grid->rows[0].push_back(std::make_shared<Text>(header));
    graph_grid->rows[1].push_back(body);

    return true;
  }

  void prepare_instances_rec(FigurePtr figure, int2 pos, std::vector<Instance> &instances);
  void LinePlot::prepare_instances(int2 pos, std::vector<Instance> &out_instances)
  {
    prepare_instances_rec(graph_grid, pos, out_instances);
  }

  int2 LinePlot::calculateSize(int2 force_size)
  {
    if (!is_valid_size(force_size) && is_valid_size(size))
      force_size = size;
    
    size = graph_grid->calculateSize(force_size);
    return size;
  }
}