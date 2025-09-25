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

  void LineGraph::prepareInstances(int2 pos, std::vector<Instance> &out_instances)
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
      inst.prim = &line;
      inst.data.pos = pos;
      inst.data.size = size;
      out_instances.push_back(inst);
    }

    for (auto &point : points)
    {
      Instance inst;
      inst.prim = &point;
      inst.data.pos = pos;
      inst.data.size = size;
      out_instances.push_back(inst);
    }
  }

  std::string default_format_from_range(float min, float max)
  {
    float size = max - min;
    if (size < 1e-4f)
      return "%.7f";
    else if (size < 1e-3f)
      return "%.6f";
    else if (size < 1e-2f)
      return "%.5f";
    else if (size < 1e-1f)
      return "%.4f";
    else if (size < 1)
      return "%.3f";
    else if (size < 10)
      return "%.2f";
    else if (size < 100)
      return "%.1f";
    else
      return "%.0f";
  }

  float tick_rounding_from_range(float tick, float min, float max)
  {
    float size = max - min;
    if (size < 1e-4f)
      return 1e-7f*round(tick/1e-7f);
    else if (size < 1e-3f)
      return 1e-6f*round(tick/1e-6f);
    else if (size < 1e-2f)
      return 1e-5f*round(tick/1e-5f);
    else if (size < 1e-1f)
      return 1e-4f*round(tick/1e-4f);
    else if (size < 1)
      return 1e-3f*round(tick/1e-3f);
    else if (size < 100)
      return 1.0f*round(tick/1.0f);
    else if (size < 1000)
      return 10.0f*round(tick/10.0f);
    else if (size < 10000)
      return 100.0f*round(tick/100.0f);
    else 
      return 1000.0f*round(tick/1000.0f);
  } 

  bool LinePlot::load(const Block *blk)
  {
    Block dummy_block;

    size = blk->get_ivec2("size", size);
    float4 background_color = blk->get_vec4("background_color", float4(1,1,1,1));

    Text default_text;
    default_text.alignment_x = TextAlignmentX::Center;
    default_text.alignment_y = TextAlignmentY::Center;
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
    header.retain_height = true;
    header.alignment_y = TextAlignmentY::Top;
    if (blk->get_block("header"))
    {
      header.load(blk->get_block("header"));
    }
    header.size = int2(size.x, 2*header.font_size);

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
        x_range = float2(std::min(x_range.x, x_values[j]), std::max(x_range.y, x_values[j]));
        y_range = float2(std::min(y_range.x, y_values[j]), std::max(y_range.y, y_values[j]));
      }
      if (graph_blk->get_block("line"))
      {
        graph.load(graph_blk->get_block("line"));
      }
      graphs.push_back(graph);
    }

    //increase range by 5% on each side to make plot look nice
    x_range.x -= (x_range.y - x_range.x) * 0.05f;
    x_range.y += (x_range.y - x_range.x) * 0.05f;
    y_range.x -= (y_range.y - y_range.x) * 0.05f;
    y_range.y += (y_range.y - y_range.x) * 0.05f;

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

    std::string x_tick_format = default_format_from_range(x_range.x, x_range.y);
    std::vector<float> x_tick_values;
    int tick_count = 5;
    if (blk->get_block("x_ticks"))
    {
      const Block *x_ticks_blk = blk->get_block("x_ticks");
      x_ticks_blk->get_arr("values", x_tick_values);
      x_tick_format = x_ticks_blk->get_string("format", x_tick_format);
      tick_count = x_ticks_blk->get_int("count", tick_count);
    }
    if (x_tick_values.empty())
    {
      float step = (x_range.y - x_range.x) / float(tick_count);
      for (int i = 0; i < tick_count; i++)
      {
        x_tick_values.push_back(tick_rounding_from_range(x_range.x + step * (i+0.5f), x_range.x, x_range.y));
      }
    }

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
    x_ticks.resize(x_tick_values.size());
    int tick_box_size = size.x/x_tick_values.size();
    for (int i = 0; i < x_tick_values.size(); i++)
    {
      x_ticks[i] = default_text;
      x_ticks[i].size = int2(tick_box_size, -1);
      x_ticks[i].retain_width = true;
      if (blk->get_block("x_ticks"))
      {
        x_ticks[i].load(blk->get_block("x_ticks"));
      }

      constexpr int MAX_TICK_TEXT_LEN = 16;
      char buf[MAX_TICK_TEXT_LEN];
      snprintf(buf, MAX_TICK_TEXT_LEN, x_tick_format.c_str(), x_tick_values[i]);
      x_ticks[i].text = buf;
      x_ticks[i].calculateSize();
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
    y_label.size = int2(1000, size.y);
    if (blk->get_block("y_label"))
    {
      y_label.load(blk->get_block("y_label"));
    }

    std::string y_tick_format = default_format_from_range(y_range.x, y_range.y);
    std::vector<float> y_tick_values;
    tick_count = 5;
    if (blk->get_block("y_ticks"))
    {
      const Block *y_ticks_blk = blk->get_block("y_ticks");
      y_ticks_blk->get_arr("values", y_tick_values);
      y_tick_format = y_ticks_blk->get_string("format", y_tick_format);
      tick_count = y_ticks_blk->get_int("count", tick_count);
    }
    if (y_tick_values.empty())
    {
      float step = (y_range.y - y_range.x) / float(tick_count);
      for (int i = 0; i < tick_count; i++)
      {
        y_tick_values.push_back(tick_rounding_from_range(y_range.x + step * (i+0.5f), y_range.x, y_range.y));
      }
    }

    y_tick_lines.resize(y_tick_values.size());
    for (int i = 0; i < y_tick_values.size(); i++)
    {
      y_tick_lines[i] = default_line;
      if (blk->get_block("y_tick_lines"))
      {
        y_tick_lines[i].load(blk->get_block("y_tick_lines"));
      }
      y_tick_lines[i].start = float2(0, 1-(y_tick_values[i] - y_range.x) / (y_range.y - y_range.x));
      y_tick_lines[i].end = float2(1, 1-(y_tick_values[i] - y_range.x) / (y_range.y - y_range.x));
    }
    y_ticks.resize(y_tick_values.size());
    tick_box_size = size.y/y_tick_values.size();
    for (int i = 0; i < y_tick_values.size(); i++)
    {
      y_ticks[i] = default_text;
      y_ticks[i].size = int2(1000,-1);
      if (blk->get_block("y_ticks"))
      {
        y_ticks[i].load(blk->get_block("y_ticks"));
      }

      constexpr int MAX_TICK_TEXT_LEN = 16;
      char buf[MAX_TICK_TEXT_LEN];
      snprintf(buf, MAX_TICK_TEXT_LEN, y_tick_format.c_str(), y_tick_values[i]);
      y_ticks[i].text = buf;

      int2 real_size = y_ticks[i].calculateSize();
    }

    std::shared_ptr<Collage> body = std::make_shared<Collage>();
    body->size = int2(size.x, size.y);
    {
      std::shared_ptr<PrimitiveFill> fill = std::make_shared<PrimitiveFill>();
      fill->size = size;
      fill->color = background_color;
      Collage::Element elem;
      elem.pos = int2(0,0);
      elem.size = int2(size.x, size.y);
      elem.figure = fill;
      body->elements.push_back(elem);
    }
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
      int offset = 0.5f*fmax(size.x, size.y)*x_axis.thickness;
      Collage::Element elem;
      elem.pos = int2(0,offset);
      elem.size = int2(size.x, size.y-offset);
      elem.figure = std::make_shared<Line>(x_axis);
      body->elements.push_back(elem);
    }
    {
      int offset = 0.5f*fmax(size.x, size.y)*y_axis.thickness;
      Collage::Element elem;
      elem.pos = int2(offset,0);
      elem.size = int2(size.x-offset, size.y);
      elem.figure = std::make_shared<Line>(y_axis);
      body->elements.push_back(elem);
    }

    std::shared_ptr<Collage> y_ticks_collage = std::make_shared<Collage>();
    y_ticks_collage->elements.resize(y_tick_values.size());
    int y_ticks_collage_max_w = 0;
    for (int i = 0; i < y_tick_values.size(); i++)
    {
      int center = size.y * (y_tick_values[i] - y_range.x) / (y_range.y - y_range.x);
      Collage::Element elem;
      elem.pos = int2(0, size.y - center - y_ticks[i].size.y);
      elem.size = y_ticks[i].size;
      elem.figure = std::make_shared<Text>(y_ticks[i]);
      y_ticks_collage->elements[i] = elem;
      y_ticks_collage_max_w = std::max(y_ticks_collage_max_w, y_ticks[i].size.x);
    }

    PrimitiveFill y_axis_separator;
    y_axis_separator.size = int2(y_label.font_size/2, size.y);
    y_axis_separator.color = background_color;

    std::shared_ptr<Grid> y_axis_grid = std::make_shared<Grid>();
    y_axis_grid->rows.resize(1);
    y_axis_grid->rows[0].push_back(std::make_shared<Text>(y_label));
    y_axis_grid->rows[0].push_back(std::make_shared<PrimitiveFill>(y_axis_separator));
    y_axis_grid->rows[0].push_back(y_ticks_collage);
    y_axis_grid->rows[0].push_back(std::make_shared<PrimitiveFill>(y_axis_separator));
    int y_axis_grid_width = y_axis_grid->calculateSize().x;
    y_axis_grid_width = y_axis_grid->calculateSize().x;

    std::shared_ptr<Collage> x_ticks_collage = std::make_shared<Collage>();
    x_ticks_collage->elements.resize(x_tick_values.size()+1);    
    {
      auto pf = std::make_shared<PrimitiveFill>();
      pf->color = background_color;
      Collage::Element elem;
      elem.pos = int2(0,0.5f*x_ticks[0].font_size);
      elem.size = int2(y_axis_grid_width + size.x, 1);
      elem.figure = pf;
      x_ticks_collage->elements[0] = elem;
    }
    for (int i = 0; i < x_tick_values.size(); i++)
    {
      int sh = x_ticks[i].size.x/2;
      int center = size.x * (x_tick_values[i] - x_range.x) / (x_range.y - x_range.x);
      Collage::Element elem;
      elem.pos = int2(y_axis_grid_width+center-sh,0);
      elem.size = x_ticks[i].size;
      elem.figure = std::make_shared<Text>(x_ticks[i]);
      x_ticks_collage->elements[i+1] = elem;
    }

    std::shared_ptr<Grid> graph_grid = std::make_shared<Grid>();
    graph_grid->rows.resize(3);
    graph_grid->rows[0].push_back(y_axis_grid);
    graph_grid->rows[0].push_back(body);
    graph_grid->rows[1].push_back(x_ticks_collage);
    graph_grid->rows[2].push_back(std::make_shared<Text>(x_label));

    std::shared_ptr<Grid> full_graph_grid = std::make_shared<Grid>();
    full_graph_grid->rows.resize(2);
    full_graph_grid->rows[0].push_back(std::make_shared<Text>(header));
    full_graph_grid->rows[1].push_back(graph_grid);

    int2 sz_grid = full_graph_grid->calculateSize();
    float pad = 0.025f;
    int2 sz_full = int2(sz_grid.x*(1+2*pad), sz_grid.y*(1+2*pad));
    int2 pos_plot = int2(sz_grid.x*pad, sz_grid.y*pad);

    full_graph_collage = std::make_shared<Collage>();
    full_graph_collage->elements.resize(2);
    full_graph_collage->elements[0].pos = int2(0,0);
    full_graph_collage->elements[0].size = sz_full;
    full_graph_collage->elements[0].figure = std::make_shared<PrimitiveFill>(background);
    full_graph_collage->elements[1].pos = pos_plot;
    full_graph_collage->elements[1].size = sz_grid;
    full_graph_collage->elements[1].figure = full_graph_grid;

    return true;
  }
  
  void LinePlot::prepareInstances(int2 pos, std::vector<Instance> &out_instances)
  {
    full_graph_collage->prepareInstances(pos, out_instances);
  }

  int2 LinePlot::calculateSize(int2 force_size)
  {
    if (!is_valid_size(force_size) && is_valid_size(size))
      force_size = size;
    
    size = full_graph_collage->calculateSize(force_size);
    return size;
  }
}