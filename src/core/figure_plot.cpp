#include "figure.h"
#include "templates.h"
#include "renderer.h"
#include "font.h"
#include "csv/csv.h"

#include <cstdio>

namespace LiteFigure
{
	REGISTER_ENUM(LegendPosition,
				  ([]()
				   { return std::vector<std::pair<std::string, unsigned>>{
             {"None", (unsigned)LegendPosition::None},
						 {"TopLeft", (unsigned)LegendPosition::TopLeft},
						 {"TopRight", (unsigned)LegendPosition::TopRight},
             {"BottomLeft", (unsigned)LegendPosition::BottomLeft},
             {"BottomRight", (unsigned)LegendPosition::BottomRight},
             {"InsideGraph", (unsigned)LegendPosition::InsideGraph},
					 }; })());

	REGISTER_ENUM(ColorPalette,
				  ([]()
				   { return std::vector<std::pair<std::string, unsigned>>{
             {"Gray10", (unsigned)ColorPalette::Gray10},
						 {"Set1", (unsigned)ColorPalette::Set1},
						 {"Set2", (unsigned)ColorPalette::Set2},
					 }; })());

  std::vector<float4> get_palette(ColorPalette type)
  {
    std::vector<float4> rp = {float4(1,0,0,1)};
    switch (type)
    {
    case ColorPalette::Gray10:
      rp =   {float4(0.0,0.0,0.0,1), float4(0.1,0.1,0.1,1), float4(0.2,0.2,0.2,1), float4(0.3,0.3,0.3,1), float4(0.4,0.4,0.4,1),
              float4(0.5,0.5,0.5,1), float4(0.6,0.6,0.6,1), float4(0.7,0.7,0.7,1), float4(0.8,0.8,0.8,1), float4(0.9,0.9,0.9,1)};
      break;
    case ColorPalette::Set1:
      rp =   {float4(0.89411765, 0.10196078, 0.10980392, 1), float4(0.21568627, 0.49411765, 0.72156863, 1), float4(0.30196078, 0.68627451, 0.29019608, 1), 
              float4(0.59607843, 0.30588235, 0.63921569, 1), float4(1.0       , 0.49803922, 0.        , 1), float4(1.0       , 1.0       , 0.2       , 1),
              float4(0.65098039, 0.3372549 , 0.15686275, 1), float4(0.96862745, 0.50588235, 0.74901961, 1), float4(0.6       , 0.6       , 0.6       , 1)};
      break;
    case ColorPalette::Set2:
      rp =   {float4(0.4       , 0.76078431, 0.64705882, 1), float4(0.98823529, 0.55294118, 0.38431373, 1), float4(0.55294118, 0.62745098, 0.79607843, 1), 
              float4(0.90588235, 0.54117647, 0.76470588, 1), float4(0.65098039, 0.84705882, 0.32941176, 1), float4(1.0       , 0.85098039, 0.18431373, 1),
              float4(0.89803922, 0.76862745, 0.58039216, 1), float4(0.70196078, 0.70196078, 0.70196078, 1)};
      break;
    }

    //gamma correction for row palette colors
    //can be done in compile time, but who cares
    for (auto &v : rp)
      v = float4(pow(v.x,2.2f), pow(v.y,2.2f), pow(v.z,2.2f), pow(v.w,2.2f));

    return rp;
  }

  bool LineGraph::load_line_params(const Block *line_blk)
  {
    if (!line_blk)
      return true;

    size = line_blk->get_ivec2("size", size);
    color = line_blk->get_vec4("color", color);
    thickness = line_blk->get_double("thickness", thickness);
    use_points = line_blk->get_bool("use_points", use_points);
    point_size = line_blk->get_double("point_size", point_size);

    return true;
  }

  bool LineGraph::load_text_params(const Block *text_blk)
  {
    if (!text_blk)
      return true;
    
    return base_text.load(text_blk);
  }

  bool LineGraph::load(const Block *blk)
  {
    load_line_params(blk->get_block("line"));
    load_text_params(blk->get_block("text"));
    
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

    blk->get_arr("labels", labels_str);

    if (!(labels_str.size() == 0 || labels_str.size() == values.size()))
    {
      printf("[LineGraph::load] if labels array is present, it must have the same size as values\n");
      return false;
    }

    labels_from_y_values = blk->get_bool("labels_from_y_values", labels_from_y_values);
    if (labels_from_y_values && labels_str.size() != 0)
    {
      printf("[LineGraph::load] both labels and labels_from_y_values are present, explicit labels array will be used\n");
      labels_from_y_values = false;
    }

    return true;
  }

  void LineGraph::rebuid()
  {
    line_graph_collage = std::make_shared<Collage>();
    for (int i=0;i<values.size()-1;i++)
    {
      std::shared_ptr<Line> line = std::make_shared<Line>();
      line->start = values[i];
      line->end = values[i+1];
      line->color = color;
      line->size = size;
      line->thickness = thickness;
      line_graph_collage->elements.push_back(Collage::Element(int2(0,0), size, line));
    }
    if (use_points)
    {
      for (int i=0;i<values.size();i++)
      {
        std::shared_ptr<Circle> point = std::make_shared<Circle>();
        point->center = values[i];
        point->radius = point_size;
        point->color = color;
        point->size = size;
        line_graph_collage->elements.push_back(Collage::Element(int2(0,0), size, point));
      }
    }
    if (!labels_str.empty())
    {
      //there are the same number of labels as values
      for (int i=0;i<values.size();i++)
      {
        if (labels_str[i].empty())
          continue;
        std::shared_ptr<Text> text = std::make_shared<Text>(base_text);
        text->text = labels_str[i];
        text->retain_height = false;
        text->retain_width = false;

        int2 text_box_size = text->calculateSize();
        int off = 0.1*text_box_size.y + use_points*std::max(size.x, size.y)*point_size;
        int2 pos = int2(float2(size)*values[i]) - int2(0.5*text_box_size.x, text_box_size.y + off);
        pos = clamp(pos, int2(0,0), size - text_box_size - int2(off, off));

        line_graph_collage->elements.push_back(Collage::Element(pos, text_box_size, text));
      }
    }
  }

  int2 LineGraph::calculateSize(int2 force_size)
  {
    if (!line_graph_collage)
      rebuid();
    size = line_graph_collage->calculateSize(force_size);
    return size;
  }

  void LineGraph::prepareInstances(int2 pos, std::vector<Instance> &out_instances)
  {
    line_graph_collage->prepareInstances(pos, out_instances);
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

  std::shared_ptr<Collage> 
  LinePlot::create_legend_collage(const Block *blk, const Text &default_text,
                                  const Line &default_line, int2 full_size)
  {
    Block dummy_block;
    const Block *legend_blk = blk->get_block("legend", &dummy_block);

    float line_length = legend_blk->get_double("line_length", 0.25f);
    float horizontal_gap = legend_blk->get_double("horizontal_gap", 0.05f);
    float vertical_gap = legend_blk->get_double("vertical_gap", 0.05f);
    float line_text_gap = legend_blk->get_double("line_text_gap", 0.05f);
    float border_thickness = legend_blk->get_double("border_thickness", 0.01f);
    float4 border_color = legend_blk->get_vec4("border_color", float4(0,0,0,1));

    std::vector<const Block *> graph_blks;
    for (int i=0;i<blk->size();i++)
    {
      if (blk->get_name(i) != "graph" || !blk->get_block(i))
        continue;

      graph_blks.push_back(blk->get_block(i));
      //std::string name = graph_blk->get_string("name", "graph_" + std::to_string(graphs.size()));
    } 

    std::shared_ptr<Grid> base_grid = std::make_shared<Grid>();
    base_grid->rows.resize(graph_blks.size());
    for (int i=0;i<graph_blks.size();i++)
    {
      std::string name = graph_blks[i]->get_string("name", "Graph " + std::to_string(i));
      std::shared_ptr<Line> line = std::make_shared<Line>(default_line);
      line->size = int2(1,1);
      if (graph_blks[i]->get_block("line"))
        line->load(graph_blks[i]->get_block("line"));
      line->start = float2(0,0.5f);
      line->end = float2(line_length/(line_length+line_text_gap),0.5f);
      std::shared_ptr<Text> text = std::make_shared<Text>(default_text);

      text->text = name;
      text->retain_height = false;
      text->retain_width = false;

      if (legend_blk->get_block("text"))
        text->load(legend_blk->get_block("text"));

      base_grid->rows[i].push_back(line);
      base_grid->rows[i].push_back(text);
    }

    std::shared_ptr<Collage> full_collage = std::make_shared<Collage>();
    int2 base_size = base_grid->calculateSize();

    for (auto &row : base_grid->rows)
    {
      int2 text_size = row[1]->calculateSize();
      row[0]->size = int2((line_length+line_text_gap)*base_size.x, text_size.y);
      float th_mul = std::max(full_size.x, full_size.y)/std::max(row[0]->size.x, row[0]->size.y); 
      std::dynamic_pointer_cast<Line>(row[0])->thickness *= th_mul;
    }

    base_size = base_grid->calculateSize(int2(base_size.x*(1+line_length+line_text_gap), base_size.y));
    int2 legend_size = int2(base_size.x*(1+2*horizontal_gap), 
                          base_size.y*(1+2*vertical_gap));
    int2 base_pos = int2(base_size.x*horizontal_gap, base_size.y*vertical_gap);

    std::shared_ptr<Rectangle> border = std::make_shared<Rectangle>();
    border->color = border_color;
    border->thickness = border_thickness;
    if (legend_blk->get_block("border"))
      border->load(legend_blk->get_block("border"));

    std::shared_ptr<PrimitiveFill> background = std::make_shared<PrimitiveFill>();
    background->size = legend_size;
    background->color = float4(1,1,1,1);
    if (legend_blk->get_block("background"))
      background->load(legend_blk->get_block("background"));

    full_collage->elements.push_back(Collage::Element(int2(0,0), legend_size, background));
    full_collage->elements.push_back(Collage::Element(base_pos, base_size, base_grid));
    full_collage->elements.push_back(Collage::Element(int2(0,0), legend_size, border));
    return full_collage;
  }

  bool LinePlot::load_graphs_block(const Block *blk, const Text &default_text,
                                   const Line &default_line, int2 full_size)
  {
    std::shared_ptr<csv::Table> filtered_csv;
    const Block *data_blk = blk->get_block("data");
    if (!data_blk)
    {
      fprintf(stderr, "[LinePlot::load_graphs_block] \"data\" block not found\n");
      return false;
    }

    {
      csv::Slice slice = csv::load_csv_slice(data_blk);
      if (!slice.data || slice.getRowCount() == 0 || slice.data->columns.size() == 0)
      {
        fprintf(stderr, "[LinePlot::load_graphs_block] csv load failed\n");
        return false;
      }
      filtered_csv = slice.toTable();
    }

    std::string group_by_col = blk->get_string("group_by");
    std::string names_col = blk->get_string("names");
    std::string labels_col = blk->get_string("labels");
    std::string x_values_col = blk->get_string("x_values");
    std::string y_values_col = blk->get_string("y_values");

    std::map<std::string, std::vector<int>> group_map;
    if (group_by_col == "")
    {
      std::vector<int> all_indices(filtered_csv->row_count);
      for (int i = 0; i < filtered_csv->row_count; i++)
        all_indices[i] = i;
      group_map[""] = all_indices;
    }
    else
    {
      int group_idx = filtered_csv->get_column_idx(group_by_col);
      if (group_idx == -1)
      {
        fprintf(stderr, "[LinePlot::load_graphs_block] group_by column \"%s\" is not found\n", group_by_col.c_str());
        return false;
      }

      for (int i = 0; i < filtered_csv->row_count; i++)
        group_map[filtered_csv->columns[group_idx][i]].push_back(i);
    }

    std::vector<float> all_x_values, all_y_values;
    std::vector<std::string> all_labels;
    if (filtered_csv->get_column_idx(labels_col) != -1)
    {
      all_labels = filtered_csv->columns[filtered_csv->get_column_idx(labels_col)];
    }
    //else it is ok to have no labels

    if (filtered_csv->get_column_idx(x_values_col) != -1)
    {
      all_x_values = csv::toFloatArray(filtered_csv->columns[filtered_csv->get_column_idx(x_values_col)]);
    }
    else
    {
      fprintf(stderr, "[LinePlot::load_graphs_block] x_values column \"%s\" is not found\n", x_values_col.c_str());
      return false;
    }

    if (filtered_csv->get_column_idx(y_values_col) != -1)
    {
      all_y_values = csv::toFloatArray(filtered_csv->columns[filtered_csv->get_column_idx(y_values_col)]);
    }
    else
    {
      fprintf(stderr, "[LinePlot::load_graphs_block] y_values column \"%s\" is not found\n", y_values_col.c_str());
      return false;
    }

    auto palette = get_palette((ColorPalette)blk->get_enum("palette", (uint32_t)ColorPalette::Set1));

    int names_idx = filtered_csv->get_column_idx(names_col);
    for (auto &p : group_map)
    {
      LineGraph graph;
      graph.color = palette[graphs.size()%palette.size()];
      graph.name = names_idx == -1 ? ("Graph " + std::to_string(graphs.size())) : filtered_csv->columns[names_idx][p.second[0]];
      for (int idx : p.second)
      {
        graph.values.push_back(float2(all_x_values[idx], all_y_values[idx]));
        if (all_labels.size() > 0)
          graph.labels_str.push_back(all_labels[idx]);
      }
      graph.load_line_params(blk->get_block("line"));
      graph.load_text_params(blk->get_block("text"));
      graphs.push_back(graph);
    }

    return true;
  }

  bool LinePlot::load(const Block *blk)
  {
    constexpr int MAX_TICK_TEXT_LEN = 16;

    Block dummy_block;

    size = blk->get_ivec2("size", size);
    float4 background_color = blk->get_vec4("background_color", float4(1,1,1,1));
    LegendPosition legend_position = LegendPosition::None;
    float2 legend_pos = float2(1,1);
    if (blk->get_block("legend"))
    {
      const Block *legend_blk = blk->get_block("legend");
      legend_position = (LegendPosition)legend_blk->get_enum("position", (uint32_t)LegendPosition::InsideGraph);
      legend_pos = legend_blk->get_vec2("pos", legend_pos);
    }

    Text default_text;
    default_text.alignment_x = TextAlignmentX::Center;
    default_text.alignment_y = TextAlignmentY::Center;
    default_text.color = blk->get_vec4("text_color", float4(0,0,0,1));
    default_text.font_name = blk->get_string("font_name", "Times-Roman");
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
      if (!blk->get_block(i))
        continue;

      if (blk->get_name(i) == "graph") //load graph directly from data
      {
        const Block *graph_blk = blk->get_block(i);
        LineGraph graph;
        graph.base_text = default_text;
        bool graph_is_ok = graph.load(graph_blk);
        if (!graph_is_ok)
          continue;
    
        graphs.push_back(graph);
      }
      else if (blk->get_name(i) == "graphs") //load one or multiple line graphs from csv or similar data
      {
        bool graphs_loaded = load_graphs_block(blk->get_block(i), default_text, default_line, size);
        if (!graphs_loaded)
          printf("[LinePlot] Warning: failed to load line graphs from \"graphs\" block\n");
      }
    }

    for (const auto &graph : graphs)
    {
      for (int j = 0; j < graph.values.size(); j++)
      {
        x_range = float2(std::min(x_range.x, graph.values[j].x), std::max(x_range.y, graph.values[j].x));
        y_range = float2(std::min(y_range.x, graph.values[j].y), std::max(y_range.y, graph.values[j].y));
      }
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
      graph.size = size;
      graph.rebuid();
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

      char buf[MAX_TICK_TEXT_LEN];
      snprintf(buf, MAX_TICK_TEXT_LEN, y_tick_format.c_str(), y_tick_values[i]);
      y_ticks[i].text = buf;

      int2 real_size = y_ticks[i].calculateSize();
    }

    // add labels from y values if needed
    for (auto &graph : graphs)
    {
      if (!graph.labels_from_y_values)
        continue;
      for (float2 val : graph.values)
      {
        float y_real = (1-val.y) * (y_range.y - y_range.x) + y_range.x;
        char buf[MAX_TICK_TEXT_LEN];
        snprintf(buf, MAX_TICK_TEXT_LEN, y_tick_format.c_str(), y_real);
        graph.labels_str.push_back(buf);
      }
      graph.rebuid();
    }

    FigurePtr legend = create_legend_collage(blk, default_text, default_line, size);

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
    if (legend_position == LegendPosition::InsideGraph)
    {
      int2 legend_size = legend->calculateSize();
      int2 offset = min(int2(legend_pos*float2(size)), size - legend_size - int2(1,1));
      printf("legend pos %f %f\n", legend_pos.x, legend_pos.y);
      Collage::Element elem;
      elem.pos = offset;
      elem.size = legend_size;
      elem.figure = legend;
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
    if (legend_position == LegendPosition::TopLeft)
    {
      full_graph_grid->rows[1].push_back(legend);
    }
    else if (legend_position == LegendPosition::BottomLeft)
    {
      int2 legend_size = legend->calculateSize();
      std::shared_ptr<Collage> legend_collage = std::make_shared<Collage>();
      legend_collage->elements.push_back(Collage::Element{int2(0,size.y - legend_size.y), legend_size, legend});
      full_graph_grid->rows[1].push_back(legend_collage);
    }
    full_graph_grid->rows[1].push_back(graph_grid);
    if (legend_position == LegendPosition::TopRight)
    {
      full_graph_grid->rows[1].push_back(legend);
    }
    else if (legend_position == LegendPosition::BottomRight)
    {
      int2 legend_size = legend->calculateSize();
      std::shared_ptr<Collage> legend_collage = std::make_shared<Collage>();
      legend_collage->elements.push_back(Collage::Element{int2(0,size.y - legend_size.y), legend_size, legend});
      full_graph_grid->rows[1].push_back(legend_collage);
    }

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
    //all text must be rendered on top of ther elements
    std::vector<Instance> local_instances;
    full_graph_collage->prepareInstances(pos, local_instances);

    //first, add everything except text
    for (auto &it : local_instances)
    {
      if (it.prim->getType() != FigureType::Glyph && it.prim->getType() != FigureType::Text)
        out_instances.push_back(it);
    }

    //then text
    for (auto &it : local_instances)
    {
      if (it.prim->getType() == FigureType::Glyph || it.prim->getType() == FigureType::Text)
        out_instances.push_back(it);
    }
  }

  int2 LinePlot::calculateSize(int2 force_size)
  {
    if (!is_valid_size(force_size) && is_valid_size(size))
      force_size = size;
    
    size = full_graph_collage->calculateSize(force_size);
    return size;
  }
}