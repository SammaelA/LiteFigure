#pragma once
#include <vector>
#include <string>
#include <memory>

#include "blk/blk.h"

namespace csv
{
  struct Table
  {
    using BaseColumn = std::vector<std::string>;

    inline int get_column_idx(const std::string &name) const
    {
      for (int i = 0; i < header.size(); i++)
        if (header[i] == name)
          return i;
      return -1;
    }
    BaseColumn &operator[](int idx) { return idx < 0 ? empty_column : columns[idx]; }
    const BaseColumn &operator[](int idx) const { return idx < 0 ? empty_column : columns[idx]; }
    BaseColumn &operator[](const std::string &name) { return operator[](get_column_idx(name)); }
    const BaseColumn &operator[](const std::string &name) const { return operator[](get_column_idx(name)); }

    std::vector<std::string> header; // column names
    std::vector<BaseColumn> columns;
    BaseColumn empty_column;
    int row_count = 0;
  };

  struct Slice
  {
    Slice() = default;
    Slice(const Slice &rhs) = default;
    Slice(const std::shared_ptr<Table> _data) : data(_data)
    {
      row_mask.resize(data->row_count, true);
    }

    int getRowCount() const;
    std::shared_ptr<Table> toTable() const;

    const std::shared_ptr<Table> data;
    std::vector<bool> row_mask;
  };

  class Filter
  {
  public:
    virtual ~Filter() = default;
    virtual void filter(Slice &slice) const = 0;
  };

  class FilterExactMatch : public Filter
  {
  public:
    FilterExactMatch(const std::string &_column_name, const std::string &_value, bool _exclude = false) : 
      column_name(_column_name), values{ _value }, exclude(_exclude) {}
    FilterExactMatch(const std::string &_column_name, const std::vector<std::string> &_values, bool _exclude = false) : 
      column_name(_column_name), values(_values), exclude(_exclude) {}
    void filter(Slice &slice) const override;
  private:
    bool exclude; // if true, exclude matched rows
    std::string column_name;
    std::vector<std::string> values;
  };

  class FilterRegexMatch : public Filter
  {
  public:
    FilterRegexMatch(const std::string &_column_name, const std::string &_regex, bool _exclude = false) : 
      column_name(_column_name), regex(_regex), exclude(_exclude) {}
    void filter(Slice &slice) const override;
  private:
    bool exclude; // if true, exclude matched rows
    std::string column_name;
    std::string regex;
  };

  class FilterRangeMatch : public Filter
  {
  public:
    FilterRangeMatch(const std::string &_column_name, double _min, double _max, bool _exclude = false) : 
      column_name(_column_name), min(_min), max(_max), exclude(_exclude) {}
    void filter(Slice &slice) const override;
  private:
    bool exclude; // if true, exclude matched rows
    std::string column_name;
    double min;
    double max;
  };

  std::shared_ptr<Table> load_csv(const std::string &filename);
  void save_csv(const std::string &filename, const Table &data, bool in_quotes = true);
  void print_csv(const Table &data, int max_rows = -1);

  std::shared_ptr<Filter> load_filter(const Block *blk);
  Slice load_csv_slice(const Block *blk);

  std::vector<int>   toIntArray(const Table::BaseColumn &column, int default_value = 0);
  std::vector<float> toFloatArray(const Table::BaseColumn &column, float default_value = 0);
}