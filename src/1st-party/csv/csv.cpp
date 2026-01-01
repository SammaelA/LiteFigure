#include "csv.h"
#include "strings.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>

namespace csv
{
  void save_csv(const std::string &filename, const Table &data, bool in_quotes)
  {
    std::ofstream fs(filename);
    for (int i = 0; i < data.header.size(); i++)
    {
      if (i > 0)
        fs << ",";
      if (in_quotes)
        fs << "\"" << data.header[i] << "\"";
      else
        fs << data.header[i];
    }
    fs << "\n";

    for (int i = 0; i < data.row_count; i++)
    {
      for (int j = 0; j < data.columns.size(); j++)
      {
        if (j > 0)
          fs << ",";
        if (in_quotes)
          fs << "\"" << data.columns[j][i] << "\"";
        else
          fs << data.columns[j][i];
      }
      fs << "\n";
    }

    fs.flush();
    fs.close();
  }

  std::shared_ptr<Table> load_csv(const std::string &filename)
  {
    std::shared_ptr<Table> data = std::make_shared<Table>();

    std::ifstream fs(filename);
    std::string line;
    std::string default_str = "NaN";

    constexpr int STATE_BASE = 0;
    constexpr int STATE_QUOTE = 1;
    constexpr int STATE_QQ = 2;
    constexpr int STATE_SPACE = 3;
    
    char buf[8192];

    int expected_columns_count = -1;
    while (std::getline(fs, line))
    {
      line += ",";
      int col = 0;
      int n = 0;
      int p = 0;
      int state = STATE_SPACE;
      for (int i = 0; i < line.size(); i++)
      {
        if (col == data->columns.size())
          data->columns.push_back(std::vector<std::string>());
        if (state == STATE_SPACE || state == STATE_BASE || state == STATE_QQ)
        {
          if (state == STATE_SPACE)
          {
            if (line[i] == ' ')
              continue;
            else
              state = STATE_BASE;
          }
          
          if (line[i] == ',')
          {
            state = STATE_SPACE;
            buf[p] = 0;
            data->columns[col].push_back(std::string(buf));
            col++;
            p = 0;
          }
          else if (line[i] == '"')
          {
            state = STATE_QUOTE;
            if (state == STATE_QQ)
              buf[p++] = line[i];
          }
          else 
          {
            buf[p++] = line[i];
          }
        }
        else if (state == STATE_QUOTE)
        {
          if (line[i] == '"')
          {
            state = STATE_QQ;
          }
          else
          {
            buf[p++] = line[i];
          }
        }
      }

      if (expected_columns_count == -1)
      {
        expected_columns_count = col;
        for (int i=0;i<expected_columns_count;i++)
        {
          data->header.push_back(data->columns[i][0]);
          data->columns[i].erase(data->columns[i].begin());
        }
      }
      else
      {
        for (int i = col; i < expected_columns_count; i++)
        {
          data->columns[i].push_back(default_str);
        }
      }
    }
    data->row_count = data->columns[0].size();

    return data;
  }

  void print_csv(const Table &data, int max_rows)
  {
    if (max_rows < 0)
      max_rows = data.row_count;
    
    if (data.header.size() > 0)
    {
      for (int i = 0; i < data.header.size(); i++)
        printf("%s ", data.header[i].c_str());
      printf("\n\n");
    }

    for (int i = 0; i < max_rows; i++)
    {
      for (int j = 0; j < data.columns.size(); j++)
      {
        printf("%s ", data.columns[j][i].c_str());
      }
      printf("\n");
    }
  }

  int Slice::getRowCount() const
  {
    int count = 0;
    for (int i = 0; i < row_mask.size(); i++)
      count += row_mask[i];
    return count;
  }

  std::shared_ptr<Table> Slice::toTable() const
  {
    if (!data)
      return nullptr;

    std::shared_ptr<Table> new_table = std::make_shared<Table>();
    new_table->columns.resize(data->columns.size(), std::vector<std::string>(getRowCount()));
    new_table->header = data->header;
    new_table->row_count = getRowCount();

    for (int i = 0; i < data->columns.size(); i++)
    {
      int idx = 0;
      for (int j = 0; j < data->row_count; j++)
      {
        if (row_mask[j])
          new_table->columns[i][idx++] = data->columns[i][j];
      }
    }
    return new_table;
  }

  void FilterExactMatch::filter(Slice &slice) const
  {
    int col_id = slice.data->get_column_idx(column_name);
    if (col_id == -1)
      return;
    for (int row = 0; row < slice.data->row_count; row++)
    {
      if (!slice.row_mask[row])
        continue;
      slice.row_mask[row] = exclude;
      for (int i = 0; i < values.size(); i++)
      {
        if (slice.data->columns[col_id][row] == values[i])
        {
          slice.row_mask[row] = !exclude;
          break;
        }
      }
    }
  }

  void FilterRegexMatch::filter(Slice &slice) const
  {
    int col_id = slice.data->get_column_idx(column_name);
    std::regex RX(regex);
    if (col_id == -1)
      return;
    for (int row = 0; row < slice.data->row_count; row++)
    {
      if (!slice.row_mask[row])
        continue;
      slice.row_mask[row] = std::regex_match(slice.data->columns[col_id][row], RX) ? !exclude : exclude;
    }
  }

  void FilterRangeMatch::filter(Slice &slice) const
  {
    int col_id = slice.data->get_column_idx(column_name);
    if (col_id == -1)
      return;
    for (int row = 0; row < slice.data->row_count; row++)
    {
      if (!slice.row_mask[row])
        continue;
      slice.row_mask[row] = exclude;
      double val = std::stod(slice.data->columns[col_id][row]);
      if (val >= min && val <= max)
        slice.row_mask[row] = !exclude;
    }
  }

  std::shared_ptr<Filter> load_filter(const Block *blk)
  {
    bool exclude = blk->get_bool("exclude", false);
    std::string column_name = blk->get_string("column", "");
    if (column_name == "")
    {
      fprintf(stderr, "unable to load filter, no column specified\n");
      return nullptr;
    }
    std::vector<std::string> values;
    if (blk->get_arr("values", values))
    {
      return std::make_shared<FilterExactMatch>(column_name, values, exclude);
    }
    else if (blk->get_string("value") != "")
    {
      return std::make_shared<FilterExactMatch>(column_name, blk->get_string("value"), exclude);
    }
    else if (blk->get_string("regex") != "")
    {
      return std::make_shared<FilterRegexMatch>(column_name, blk->get_string("regex"), exclude);
    }
    else if (blk->get_id("range") != -1 && blk->get_type("range") == Block::ValueType::VEC2)
    {
      float2 range = blk->get_vec2("range");
      return std::make_shared<FilterRangeMatch>(column_name, range.x, range.y, exclude);
    }

    fprintf(stderr, "unable to load filter, no range, value, value list or regex specified\n");
    return nullptr;
  }

  Slice load_csv_slice(const Block *blk)
  {
    std::string path = blk->get_string("path", "");
    if (path == "")
    {
      fprintf(stderr, "unable to load csv file, no filename specified\n");
      return {};
    }
    if (!std::filesystem::exists(path))
    {
      fprintf(stderr, "unable to load csv file, file \"%s\" does not exist\n", path.c_str());
      return {};
    }

    Slice slice = load_csv(path);
    if (slice.data == nullptr || slice.data->row_count == 0 || slice.data->columns.size() == 0)
    {
      fprintf(stderr, "unable to read datafrom csv file \"%s\"\n", path.c_str());
      return {};
    }
    
    std::vector<std::shared_ptr<Filter>> filters;
    for (int i=0;i<blk->size();i++)
    {
      if (blk->get_type(i) == Block::ValueType::BLOCK)
      {
        auto filter = load_filter(blk->get_block(i));
        if (filter)
          filters.push_back(filter);
      }
    }

    for (auto &filter : filters)
      filter->filter(slice);

    return slice;
  }

  std::vector<int> toIntArray(const Table::BaseColumn &column, int default_value)
  {
    std::vector<int> result(column.size());
    for (int i = 0; i < column.size(); i++)
    {
      char *end_c = nullptr;
      uint32_t value = strtol(column[i].c_str(), &end_c, 10);
      result[i] = (end_c == column[i].c_str()) ? default_value : value;
    }
    return result;
  }

  std::vector<float> toFloatArray(const Table::BaseColumn &column, float default_value)
  {
    std::vector<float> result(column.size());
    for (int i = 0; i < column.size(); i++)
    {
      char *end_c = nullptr;
      float value = strtod(column[i].c_str(), &end_c);
      result[i] = (end_c == column[i].c_str()) ? default_value : value;
    }
    return result;
  }
}