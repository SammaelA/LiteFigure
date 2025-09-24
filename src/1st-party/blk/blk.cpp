#include "blk.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>

using LiteMath::cross;
using LiteMath::dot;
using LiteMath::float2;
using LiteMath::float3;
using LiteMath::float3x3;
using LiteMath::float4;
using LiteMath::float4x4;
using LiteMath::int2;
using LiteMath::int3;
using LiteMath::int4;
using LiteMath::length;
using LiteMath::normalize;
using LiteMath::to_float3;
using LiteMath::to_float4;
using LiteMath::uint2;
using LiteMath::uint3;
using LiteMath::uint4;

int cur_line = 0;
bool in_comment_assume = false;
bool in_comment = false;

struct EnumInfo
{
  std::string name;
  std::vector<std::pair<std::string, unsigned>> raw_info;
  std::map<std::string, unsigned> id_by_name;
  std::map<unsigned,    unsigned> id_by_val;
  std::vector<const char *> names;
  std::vector<unsigned> values;
};

int MAX_ENUMS = 1024;
std::vector<EnumInfo>& get_enum_info() {
    static std::vector<EnumInfo> g_enum_info;
    return g_enum_info;
}
std::map<std::string, unsigned>& get_enum_info_by_name() {
    static std::map<std::string, unsigned> g_enum_info_by_name;
    return g_enum_info_by_name;
}

void register_enum_info(const std::string &name, const std::vector<std::pair<std::string, unsigned>> &values)
{
  if (get_enum_info().capacity() == 0)
    get_enum_info().reserve(MAX_ENUMS);
  if (get_enum_info().size() >= MAX_ENUMS)
  {
    fprintf(stderr, "[register_enum_info::ERROR] too many enums\n");
    return;
  }
  auto it = get_enum_info_by_name().find(name);
  if (it != get_enum_info_by_name().end())
    fprintf(stderr, "[register_enum_info::ERROR] enum %s already registered\n", name.c_str());
  else
  {
    get_enum_info().emplace_back();
    get_enum_info_by_name()[name] = get_enum_info().size() - 1;
    EnumInfo &info = get_enum_info().back();
    info.name = name;
    info.raw_info = values;
    for (unsigned i = 0; i < values.size(); i++)
    {
      info.names.push_back(info.raw_info[i].first.c_str());
      info.values.push_back(info.raw_info[i].second);
      bool is_valid_name = true;
      for (unsigned j = 0; j < values[i].first.size(); j++)
      {
        bool lower = values[i].first[j] >= 'a' && values[i].first[j] <= 'z';
        bool upper = values[i].first[j] >= 'A' && values[i].first[j] <= 'Z';
        bool digit = values[i].first[j] >= '0' && values[i].first[j] <= '9';
        bool underscore = values[i].first[j] == '_';
        if (!lower && !upper && (!digit || j == 0) && !underscore)
        {
          fprintf(stderr, "[register_enum_info::ERROR] enum %s name %s has invalid character %c\n", 
                  name.c_str(), values[i].first.c_str(), values[i].first[j]);
          is_valid_name = false;
          break;
        }
      }
      if (!is_valid_name)
        continue;
      if (info.id_by_name.find(values[i].first) != info.id_by_name.end())
        fprintf(stderr, "[register_enum_info::ERROR] enum %s has repeated name %s\n", name.c_str(), values[i].first.c_str());
      info.id_by_name[values[i].first] = i;

      if (info.id_by_val.find(values[i].second) != info.id_by_val.end())
        fprintf(stderr, "[register_enum_info::ERROR] enum %s has repeated value %u\n", name.c_str(), values[i].second);
      info.id_by_val[values[i].second] = i;
    }
  }
}

const std::vector<std::pair<std::string, unsigned>> *get_enum_info(const std::string &name)
{
  auto it = get_enum_info_by_name().find(name);
  if (it != get_enum_info_by_name().end())
    return &(get_enum_info()[it->second].raw_info);
  else
    return nullptr;
}

std::vector<const char *> *get_enum_names(unsigned type_id)
{
  return &(get_enum_info()[type_id].names);
}

BlkEnumLoader::BlkEnumLoader(const std::string &name, const std::vector<std::pair<std::string, unsigned>> &values)
{
  register_enum_info(name, values);
}

bool is_empty(const char c)
{
  if (c == '\n')
    cur_line++;

  if (in_comment)
  {
    if (c == '\n')
    {
      in_comment = false;

      return true;
    }
    else
      return true;
  }
  else if (!in_comment_assume && c == '/')
  {
    in_comment_assume = true;
    return true;
  }
  else if (in_comment_assume)
  {
    if (c == '/')
    {
      in_comment_assume = false;
      in_comment = true;
      return true;
    }
    else
    {
      fprintf(stderr, "line %d hanging / found", cur_line);
      in_comment_assume = false;
    }
  }

  return (c == ' ' || c == '\n' || c == '\t');
}
bool is_div(const char c)
{
  return (c == ',' || c == ';' || c == ':' || c == '=' || c == '{' || c == '}' || c == '\'' || c == '\"');
}
std::string next_token(const char *data, int &pos)
{
  std::string res;
  if (!data || data[pos] == 0)
    res = "";
  else
  {
    while (is_empty(data[pos]))
      pos++;
    if (data[pos] == 0)
      res = "";
    else
    {
      if (is_div(data[pos]))
      {
        pos++;
        res = std::string(1, data[pos - 1]);
      }
      else
      {
        const char *start = data + pos;
        int sz = 0;
        while (!is_div(data[pos]) && !is_empty(data[pos]) && data[pos] != 0)
        {
          pos++;
          sz++;
        }
        res = std::string(start, sz);
      }
    }
  }
  return res;
}


static constexpr int CHARS_COUNT = 12;
static const char *esc_chars = "abefnrtv\'\"\\?";
static const char *esc_codes = "\a\b\e\f\n\r\t\v\'\"\\\?";

//reading string, processing escape sequences
std::string read_string(const char *data, int &cur_pos)
{
  enum class State
  {
    NORMAL,
    ESCAPE,
    OCT_1,
    OCT_2,
    HEX,
    HEX_1
  };

  std::string s;
  int len = 0;
  bool had_escape = false;
  State state = State::NORMAL;
  uint32_t cur_code = 0;
  while (data[cur_pos] != 0 && !(data[cur_pos] == '\"' && state == State::NORMAL))
  {
    if (state == State::NORMAL)
    {
      if (data[cur_pos] == '\\')
      {
        if (!had_escape)
        {
          s = std::string(data + cur_pos - len, len);
          had_escape = true;
        }
        cur_code = 0;
        state = State::ESCAPE;
      }
      else if (had_escape)
        s.push_back(data[cur_pos]);
    }
    else if (state == State::ESCAPE)
    {
      if (data[cur_pos] == 'x')
        state = State::HEX;
      else if (data[cur_pos] >= '0' && data[cur_pos] <= '7')
      {
        state = State::OCT_1;
        cur_code = data[cur_pos] - '0';
      }
      else
      {
        bool found = false;
        for (int i = 0; i < CHARS_COUNT; i++)
        {
          if (data[cur_pos] == esc_chars[i])
          {
            s.push_back(esc_codes[i]);
            found = true;
            break;
          }
        }
        if (!found)
        {
          s.push_back(data[cur_pos]);
          fprintf(stderr, "line %d unknown escape sequence", cur_line);
        }
        state = State::NORMAL;
      }
    }
    else if (state == State::OCT_1)
    {
      if (data[cur_pos] >= '0' && data[cur_pos] <= '7')
      {
        cur_code = cur_code * 8 + data[cur_pos] - '0';
        state = State::OCT_2;
      }
      else
      {
        s.push_back(char(cur_code));
        state = State::NORMAL;
      }
    }
    else if (state == State::OCT_2)
    {
      if (data[cur_pos] >= '0' && data[cur_pos] <= '7')
      {
        cur_code = cur_code * 8 + data[cur_pos] - '0';
        state = State::NORMAL;
      }
      else
      {
        s.push_back(char(cur_code));
        state = State::NORMAL;
      }
    }
    else if (state == State::HEX)
    {
      if (data[cur_pos] >= '0' && data[cur_pos] <= '9')
      {
        cur_code = cur_code * 16 + data[cur_pos] - '0';
        state = State::HEX_1;
      }
      else if (data[cur_pos] >= 'a' && data[cur_pos] <= 'f')
      {
        cur_code = cur_code * 16 + data[cur_pos] - 'a' + 10;
        state = State::HEX_1;
      }
      else if (data[cur_pos] >= 'A' && data[cur_pos] <= 'F')
      {
        cur_code = cur_code * 16 + data[cur_pos] - 'A' + 10;
        state = State::HEX_1;
      }
      else
      {
        fprintf(stderr, "line %d broken hex escape sequence", cur_line);
        s.push_back('\\');
        s.push_back('x');
        state = State::NORMAL;
      }
    }
    else if (state == State::HEX_1)
    {
      if (data[cur_pos] >= '0' && data[cur_pos] <= '9')
      {
        cur_code = cur_code * 16 + data[cur_pos] - '0';
      }
      else if (data[cur_pos] >= 'a' && data[cur_pos] <= 'f')
      {
        cur_code = cur_code * 16 + data[cur_pos] - 'a' + 10;
      }
      else if (data[cur_pos] >= 'A' && data[cur_pos] <= 'F')
      {
        cur_code = cur_code * 16 + data[cur_pos] - 'A' + 10;
      }
      else
      {
        s.push_back(char(cur_code));
        state = State::NORMAL;
      }
    }
    len++;
    cur_pos++;
  }

  if (!had_escape)
    s = std::string(data + cur_pos - len, len);

  return s;
}

// saves string with escape sequences and hex for unprintable characters
std::string save_string(const std::string &s)
{
  const char *hex_chars = "0123456789ABCDEF";
  std::string res;
  res.reserve(s.size() * 2);
  for (int i = 0; i < s.size(); i++)
  {
    bool found = false;
    for (int j = 0; j < CHARS_COUNT; j++)
    {
      if (s[i] == esc_codes[j])
      {
        res.push_back('\\');
        res.push_back(esc_chars[j]);
        found = true;
        break;
      }
    }
    if (!found && s[i] < 32)
    {
      res.push_back('\\');
      res.push_back('x');
      res.push_back(hex_chars[s[i] / 16]);
      res.push_back(hex_chars[s[i] % 16]);
    }
    else if (!found)
      res.push_back(s[i]);
  }
  return res;
}

bool load_block(const char *data, int &cur_pos, Block &b);
bool read_array(const char *data, int &cur_pos, Block::DataArray &a);
bool read_value(const char *data, int &cur_pos, Block::Value &v, const Block &b)
{
  std::string token = next_token(data, cur_pos);
  //:<type> = <description> or { <block> }
  if (token == "{" || token == "extends")
  {
    const Block *block_to_extend = nullptr;
    // extends <parent_block_name> { <block> }
    if (token == "extends")
    {
      std::string name = next_token(data, cur_pos);
      std::string next_tok = next_token(data, cur_pos);
      if (next_tok != "{")
      {
        fprintf(stderr, "line %d expected { after extends <parent_block_name>", cur_line);
        v.type = Block::ValueType::EMPTY;
        return false;
      }
      block_to_extend = b.get_block(name);
      if (!block_to_extend)
      {
        printf("Warning: block %s is set to be parent for extension, but was not found\n", name.c_str());
      }
    }
    v.bl = new Block();
    v.type = Block::ValueType::BLOCK;
    bool loaded = load_block(data, cur_pos, *(v.bl));
    if (loaded && block_to_extend)
    {
      Block *det_blk = v.bl;
      v.bl = new Block();
      v.bl->copy(block_to_extend);
      v.bl->add_detalization(*det_blk);
      delete det_blk;
    }
    return loaded;
  }
  else if (token == ":")
  { // simple value or array
    std::string type = next_token(data, cur_pos);
    if (type == "tag")
    {
      v.type = Block::ValueType::EMPTY;
      return true;
    }
    std::string eq = next_token(data, cur_pos);
    if (eq != "=")
    {
      fprintf(stderr, "line %d expected = after value type", cur_line);
      v.type = Block::ValueType::EMPTY;
      return false;
    }
    if (type == "b")
    {
      std::string val = next_token(data, cur_pos);
      v.type = Block::ValueType::BOOL;
      v.b = val == "true" || val == "True" || val == "TRUE";
    }
    else if (type == "i")
    {
      std::string val = next_token(data, cur_pos);
      v.type = Block::ValueType::INT;
      v.i = std::stol(val);
    }
    else if (type == "u" || type == "u64")
    {
      std::string val = next_token(data, cur_pos);
      v.type = Block::ValueType::UINT64;
      v.u = std::stoul(val);
    }
    else if (type == "r")
    {
      std::string val = next_token(data, cur_pos);
      v.type = Block::ValueType::DOUBLE;
      v.d = std::stod(val);
    }
    else if (type == "p2")
    {
      v.type = Block::ValueType::VEC2;
      std::string val;
      bool ok = true;
      v.v2 = float2(0, 0);

      val = next_token(data, cur_pos);
      v.v2.x = std::stod(val);

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");

      if (ok)
      {
        val = next_token(data, cur_pos);
        v.v2.y = std::stod(val);
      }
      if (!ok)
      {
        fprintf(stderr, "line %d wrong description of vector", cur_line);
        v.type = Block::ValueType::EMPTY;
        return false;
      }
    }
    else if (type == "p3")
    {
      v.type = Block::ValueType::VEC3;
      std::string val;
      bool ok = true;
      v.v3 = float3(0, 0, 0);

      val = next_token(data, cur_pos);
      v.v3.x = std::stod(val);

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");
      if (ok)
      {
        val = next_token(data, cur_pos);
        v.v3.y = std::stod(val);
      }

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");
      if (ok)
      {
        val = next_token(data, cur_pos);
        v.v3.z = std::stod(val);
      }
      if (!ok)
      {
        fprintf(stderr, "line %d wrong description of vector", cur_line);
        v.type = Block::ValueType::EMPTY;
        return false;
      }
    }
    else if (type == "p4")
    {
      v.type = Block::ValueType::VEC4;
      std::string val;
      bool ok = true;
      v.v4 = float4(0, 0, 0, 0);

      val = next_token(data, cur_pos);
      v.v4.x = std::stod(val);

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");
      if (ok)
      {
        val = next_token(data, cur_pos);
        v.v4.y = std::stod(val);
      }

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");
      if (ok)
      {
        val = next_token(data, cur_pos);
        v.v4.z = std::stod(val);
      }

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");
      if (ok)
      {
        val = next_token(data, cur_pos);
        v.v4.w = std::stod(val);
      }
      if (!ok)
      {
        fprintf(stderr, "line %d wrong description of vector", cur_line);
        v.type = Block::ValueType::EMPTY;
        return false;
      }
    }
    else if (type == "i2")
    {
      v.type = Block::ValueType::IVEC2;
      std::string val;
      bool ok = true;
      v.iv2 = int2(0, 0);

      val = next_token(data, cur_pos);
      v.iv2.x = std::stoi(val);

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");

      if (ok)
      {
        val = next_token(data, cur_pos);
        v.iv2.y = std::stoi(val);
      }
      if (!ok)
      {
        fprintf(stderr, "line %d wrong description of integer vector", cur_line);
        v.type = Block::ValueType::EMPTY;
        return false;
      }
    }
    else if (type == "i3")
    {
      v.type = Block::ValueType::IVEC3;
      std::string val;
      bool ok = true;
      v.iv3 = int3(0, 0, 0);

      val = next_token(data, cur_pos);
      v.iv3.x = std::stoi(val);

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");
      if (ok)
      {
        val = next_token(data, cur_pos);
        v.iv3.y = std::stoi(val);
      }

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");
      if (ok)
      {
        val = next_token(data, cur_pos);
        v.iv3.z = std::stoi(val);
      }
      if (!ok)
      {
        fprintf(stderr, "line %d wrong description of integer vector", cur_line);
        v.type = Block::ValueType::EMPTY;
        return false;
      }
    }
    else if (type == "i4")
    {
      v.type = Block::ValueType::IVEC4;
      std::string val;
      bool ok = true;
      v.iv4 = int4(0, 0, 0, 0);

      val = next_token(data, cur_pos);
      v.iv4.x = std::stoi(val);

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");
      if (ok)
      {
        val = next_token(data, cur_pos);
        v.iv4.y = std::stoi(val);
      }

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");
      if (ok)
      {
        val = next_token(data, cur_pos);
        v.iv4.z = std::stoi(val);
      }

      val = next_token(data, cur_pos);
      ok = ok && (val == ",");
      if (ok)
      {
        val = next_token(data, cur_pos);
        v.iv4.w = std::stoi(val);
      }
      if (!ok)
      {
        fprintf(stderr, "line %d wrong description of integer vector", cur_line);
        v.type = Block::ValueType::EMPTY;
        return false;
      }
    }
    else if (type == "m4")
    {
      v.type = Block::ValueType::MAT4;
      v.m4 = float4x4();
      std::string val;
      float mat[16];
      bool ok = true;
      int cnt = 0;

      while (cnt < 16 && ok)
      {
        val = next_token(data, cur_pos);

        mat[cnt] = std::stof(val);
        if (cnt < 15)
        {
          val = next_token(data, cur_pos);
          ok = ok && (val == ",");
        }
        cnt++;
      }
      if (ok)
      {
        v.m4 = float4x4(mat[0], mat[4], mat[8], mat[12],
                        mat[1], mat[5], mat[9], mat[13],
                        mat[2], mat[6], mat[10], mat[14],
                        mat[3], mat[7], mat[11], mat[15]);
      }
      else
      {
        fprintf(stderr, "line %d wrong description of matrix", cur_line);
        v.type = Block::ValueType::EMPTY;
        return false;
      }
    }
    else if (type.substr(0, 2) == "e_")
    {
      v.type = Block::ValueType::ENUM;

      std::string type_name = type.substr(2);
      std::string name = next_token(data, cur_pos);

      auto info_it = get_enum_info_by_name().find(type_name);
      if (info_it == get_enum_info_by_name().end())
      {
        fprintf(stderr, "line %d enum %s is not registered\n", cur_line, type_name.c_str());
        v.ev.type_id = 0;
        v.ev.val_id  = 0; //it is an error, but we can ignore it and hope the user code will deal with it
      }

      auto val_it = get_enum_info()[info_it->second].id_by_name.find(name);
      if (val_it == get_enum_info()[info_it->second].id_by_name.end())
      {
        fprintf(stderr, "line %d enum %s has no value %s\n", cur_line, type_name.c_str(), name.c_str());
        v.ev.type_id = 0;
        v.ev.val_id  = 0; //it is an error, but we can ignore it and hope the user code will deal with it
      }
      else
      {
        v.ev.type_id = info_it->second;
        v.ev.val_id  = val_it->second;
      }
    }
    else if (type == "s")
    {
      std::string par = next_token(data, cur_pos);
      if (par == "\"")
      {
        std::string s = read_string(data, cur_pos);
        if (data[cur_pos] == 0)
        {
          v.type = Block::ValueType::EMPTY;
          fprintf(stderr, "line %d expected \" at the end of a string", cur_line);
          return false;
        }
        else if (data[cur_pos] == '\"')
        {
          cur_pos++;
          v.type = Block::ValueType::STRING;
          v.s = new std::string(s);
        }
      }
    }
    else if (type == "arr")
    {
      v.type = Block::ValueType::ARRAY;
      v.a = new Block::DataArray();
      return read_array(data, cur_pos, *(v.a));
    }

    return true;
  }
  else
  {
    fprintf(stderr, "line %d expected : or { after value/block name, but %s got", cur_line, token.c_str());
    v.type = Block::ValueType::EMPTY;
    return false;
  }
}
bool read_array(const char *data, int &cur_pos, Block::DataArray &a)
{
  std::string token = next_token(data, cur_pos);
  //{ <value>, <value>, ... <value>}
  // <value> := "string" or <double>
  // all values should have the same type
  if (token == "{")
  {
    bool ok = true;
    Block::ValueType array_type = Block::ValueType::DOUBLE;
    while (ok)
    {
      Block::Value val;
      std::string tok = next_token(data, cur_pos);
      if (tok == "}")
      {
        a.type = Block::ValueType::DOUBLE;
        return true;
      }
      if (tok.empty())
        fprintf(stderr, "line %d empty token in array", cur_line);
      else if (tok == "\"")
      {
        std::string s = read_string(data, cur_pos);
        if (data[cur_pos] == 0)
        {
          val.type = Block::ValueType::EMPTY;
          fprintf(stderr, "line %d expected \" at the end of a string in string array", cur_line);
          return false;
        }
        else if (data[cur_pos] == '\"')
        {
          cur_pos++;
          val.type = Block::ValueType::STRING;
          val.s = new std::string(s);
        }
      }
      else
      {
        val.type = Block::ValueType::DOUBLE;
        val.d = std::stod(tok);
      }
      if (a.values.empty())
        array_type = val.type;
      else if (array_type != val.type)
        fprintf(stderr, "line %d array has values of diffrent types", cur_line);
      a.values.push_back(val);

      tok = next_token(data, cur_pos);
      ok = ok && (tok == ",");
      if (tok == "}")
      {
        a.type = array_type;
        return true;
      }
    }
    fprintf(stderr, "line %d expected } at the end of array", cur_line);
    return false;
  }
  else
  {
    fprintf(stderr, "line %d expected { at the start of array", cur_line);
    return false;
  }
}
bool load_block(const char *data, int &cur_pos, Block &b)
{
  bool correct = true;
  while (correct)
  {
    std::string token = next_token(data, cur_pos);
    if (token == "}")
    {
      // block closed correctly
      return correct;
    }
    else if (token == "")
    {
      // end of file
      fprintf(stderr, "line %d block loader reached end of file, } expected", cur_line);
      return false;
    }
    else
    {
      // next value
      b.names.push_back(token);
      b.values.emplace_back();
      correct = correct && read_value(data, cur_pos, b.values.back(), b);
    }
  }
  return true;
}

bool load_block_from_string(const std::string &str, Block &b)
{
  b = Block();
  if (str.empty())
    return false;
  cur_line = 0;
  int cur_pos = 0;
  const char *data = str.c_str();
  std::string token = next_token(data, cur_pos);
  if (token == "{")
  {
    return load_block(data, cur_pos, b);
  }
  else
  {
    return false;
  }
  return true;
}

bool load_block_from_file(std::string path, Block &b)
{
  b = Block();
  std::fstream f(path);
  std::stringstream iss;
  if (f.fail())
  {
    fprintf(stderr, "unable to load file %s", path.c_str());
    return false;
  }
  iss << f.rdbuf();
  std::string entireFile = iss.str();
  load_block_from_string(entireFile, b);
  return true;
}

int Block::size() const
{
  return names.size();
}

int Block::get_id(const std::string &name) const
{
  return get_next_id(name, 0);
}
int Block::get_next_id(const std::string &name, int pos) const
{
  for (int i = pos; i < names.size(); i++)
  {
    if (names[i] == name)
      return i;
  }
  return -1;
}
Block::ValueType Block::get_type(int id) const
{
  return (id >= 0 && id < size()) ? values[id].type : Block::ValueType::EMPTY;
}
Block::ValueType Block::get_type(const std::string &name) const
{
  return get_type(get_id(name));
}

int Block::get_bool(int id, bool base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::BOOL) ? values[id].b : base_val;
}
int Block::get_int(int id, int base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::INT) ? values[id].i : base_val;
}
uint64_t Block::get_uint64(int id, uint64_t base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::UINT64) ? values[id].u : base_val;
}
double Block::get_double(int id, double base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::DOUBLE) ? values[id].d : base_val;
}
float2 Block::get_vec2(int id, float2 base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::VEC2) ? values[id].v2 : base_val;
}
float3 Block::get_vec3(int id, float3 base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::VEC3) ? values[id].v3 : base_val;
}
float4 Block::get_vec4(int id, float4 base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::VEC4) ? values[id].v4 : base_val;
}
int2 Block::get_ivec2(int id, int2 base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::IVEC2) ? values[id].iv2 : base_val;
}
int3 Block::get_ivec3(int id, int3 base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::IVEC3) ? values[id].iv3 : base_val;
}
int4 Block::get_ivec4(int id, int4 base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::IVEC4) ? values[id].iv4 : base_val;
}
float4x4 Block::get_mat4(int id, float4x4 base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::MAT4) ? values[id].m4 : base_val;
}
unsigned Block::get_enum(int id, unsigned base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::ENUM) ? get_enum_info()[values[id].ev.type_id].values[values[id].ev.val_id] : base_val;
}
std::string Block::get_string(int id, std::string base_val) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::STRING && values[id].s) ? *(values[id].s) : base_val;
}
Block *Block::get_block(int id) const
{
  return (id >= 0 && id < size() && values[id].type == Block::ValueType::BLOCK) ? values[id].bl : nullptr;
}
bool Block::get_arr(int id, std::vector<double> &_values, bool replace) const
{
  if (id >= 0 && id < size() && values[id].type == Block::ValueType::ARRAY && values[id].a &&
      (values[id].a->type == DOUBLE))
  {
    if (replace)
      _values.clear();
    for (Block::Value &v : values[id].a->values)
    {
      _values.push_back(v.d);
    }
    return true;
  }
  return false;
}
bool Block::get_arr(int id, std::vector<float> &_values, bool replace) const
{
  if (id >= 0 && id < size() && values[id].type == Block::ValueType::ARRAY && values[id].a &&
      (values[id].a->type == DOUBLE))
  {
    if (replace)
      _values.clear();
    for (Block::Value &v : values[id].a->values)
    {
      _values.push_back(v.d);
    }
    return true;
  }
  return false;
}
bool Block::get_arr(int id, std::vector<int> &_values, bool replace) const
{
  if (id >= 0 && id < size() && values[id].type == Block::ValueType::ARRAY && values[id].a &&
      (values[id].a->type == DOUBLE))
  {
    if (replace)
      _values.clear();
    for (Block::Value &v : values[id].a->values)
    {
      _values.push_back(v.d);
    }
    return true;
  }
  return false;
}
bool Block::get_arr(int id, std::vector<unsigned> &_values, bool replace) const
{
  if (id >= 0 && id < size() && values[id].type == Block::ValueType::ARRAY && values[id].a &&
      (values[id].a->type == DOUBLE))
  {
    if (replace)
      _values.clear();
    for (Block::Value &v : values[id].a->values)
    {
      _values.push_back(v.d);
    }
    return true;
  }
  return false;
}
bool Block::get_arr(int id, std::vector<short> &_values, bool replace) const
{
  if (id >= 0 && id < size() && values[id].type == Block::ValueType::ARRAY && values[id].a &&
      (values[id].a->type == DOUBLE))
  {
    if (replace)
      _values.clear();
    for (Block::Value &v : values[id].a->values)
    {
      _values.push_back(v.d);
    }
    return true;
  }
  return false;
}
bool Block::get_arr(int id, std::vector<unsigned short> &_values, bool replace) const
{
  if (id >= 0 && id < size() && values[id].type == Block::ValueType::ARRAY && values[id].a &&
      (values[id].a->type == DOUBLE))
  {
    if (replace)
      _values.clear();
    for (Block::Value &v : values[id].a->values)
    {
      _values.push_back(v.d);
    }
    return true;
  }
  return false;
}
bool Block::get_arr(int id, std::vector<std::string> &_values, bool replace) const
{
  if (id >= 0 && id < size() && values[id].type == Block::ValueType::ARRAY && values[id].a &&
      (values[id].a->type == STRING))
  {
    if (replace)
      _values.clear();
    for (Block::Value &v : values[id].a->values)
    {
      if (v.s)
        _values.push_back(*(v.s));
      else
        _values.push_back("");
    }
    return true;
  }
  return false;
}

int Block::get_bool(const std::string name, bool base_val) const
{
  return get_bool(get_id(name), base_val);
}
int Block::get_int(const std::string name, int base_val) const
{
  return get_int(get_id(name), base_val);
}
uint64_t Block::get_uint64(const std::string name, uint64_t base_val) const
{
  return get_uint64(get_id(name), base_val);
}
double Block::get_double(const std::string name, double base_val) const
{
  return get_double(get_id(name), base_val);
}
float2 Block::get_vec2(const std::string name, float2 base_val) const
{
  return get_vec2(get_id(name), base_val);
}
float3 Block::get_vec3(const std::string name, float3 base_val) const
{
  return get_vec3(get_id(name), base_val);
}
float4 Block::get_vec4(const std::string name, float4 base_val) const
{
  return get_vec4(get_id(name), base_val);
}
int2 Block::get_ivec2(const std::string name, int2 base_val) const
{
  return get_ivec2(get_id(name), base_val);
}
int3 Block::get_ivec3(const std::string name, int3 base_val) const
{
  return get_ivec3(get_id(name), base_val);
}
int4 Block::get_ivec4(const std::string name, int4 base_val) const
{
  return get_ivec4(get_id(name), base_val);
}
float4x4 Block::get_mat4(const std::string name, float4x4 base_val) const
{
  return get_mat4(get_id(name), base_val);
}
unsigned Block::get_enum(const std::string name, unsigned base_val) const
{
  return get_enum(get_id(name), base_val);
}
std::string Block::get_string(const std::string name, std::string base_val) const
{
  return get_string(get_id(name), base_val);
}
Block *Block::get_block(std::string name) const
{
  return get_block(get_id(name));
}
bool Block::get_arr(const std::string name, std::vector<double> &_values, bool replace) const
{
  return get_arr(get_id(name), _values, replace);
}
bool Block::get_arr(const std::string name, std::vector<float> &_values, bool replace) const
{
  return get_arr(get_id(name), _values, replace);
}
bool Block::get_arr(const std::string name, std::vector<int> &_values, bool replace) const
{
  return get_arr(get_id(name), _values, replace);
}
bool Block::get_arr(const std::string name, std::vector<unsigned> &_values, bool replace) const
{
  return get_arr(get_id(name), _values, replace);
}
bool Block::get_arr(const std::string name, std::vector<short> &_values, bool replace) const
{
  return get_arr(get_id(name), _values, replace);
}
bool Block::get_arr(const std::string name, std::vector<unsigned short> &_values, bool replace) const
{
  return get_arr(get_id(name), _values, replace);
}
bool Block::get_arr(const std::string name, std::vector<std::string> &_values, bool replace) const
{
  return get_arr(get_id(name), _values, replace);
}

std::string double_to_string(double val)
{
  //buffer size is the same as in gcc-12 implementation of to_string
  char buffer[std::numeric_limits<double>::max_exponent10 + 20];
  snprintf(buffer, sizeof(buffer), "%g", val);
  return std::string(buffer);
}

void save_value(std::string &str, Block::Value &v);
void save_block(std::string &str, Block &b)
{
  str += "{\n";
  for (int i = 0; i < b.size(); i++)
  {
    str += b.names[i];
    save_value(str, b.values[i]);
    str += "\n";
  }
  str += "}";
}
void save_arr(std::string &str, Block::DataArray &a)
{
  str += "{ ";
  if (a.type == Block::ValueType::DOUBLE)
  {
    for (int i = 0; i < a.values.size(); i++)
    {
      str += double_to_string(a.values[i].d);
      if (i < a.values.size() - 1)
        str += ", ";
    }
  }
  else if (a.type == Block::ValueType::STRING)
  {
    for (int i = 0; i < a.values.size(); i++)
    {
      if (a.values[i].s)
        str += "\"" + save_string(*(a.values[i].s)) + "\"";
      else
        str += "\"\"";
      if (i < a.values.size() - 1)
        str += ", ";
    }
  }
  str += " }";
}
void save_value(std::string &str, Block::Value &v)
{
  if (v.type == Block::ValueType::EMPTY)
  {
    str += ":tag";
  }
  else if (v.type == Block::ValueType::BOOL)
  {
    str += ":b = ";
    str += v.b ? "true" : "false";
  }
  else if (v.type == Block::ValueType::INT)
  {
    str += ":i = ";
    str += std::to_string(v.i);
  }
  else if (v.type == Block::ValueType::UINT64)
  {
    str += ":u64 = ";
    str += std::to_string(v.u);
  }
  else if (v.type == Block::ValueType::DOUBLE)
  {
    str += ":r = ";
    str += double_to_string(v.d);
  }
  else if (v.type == Block::ValueType::VEC2)
  {
    str += ":p2 = ";
    str += double_to_string(v.v2.x) + ", " + double_to_string(v.v2.y);
  }
  else if (v.type == Block::ValueType::VEC3)
  {
    str += ":p3 = ";
    str += double_to_string(v.v3.x) + ", " + double_to_string(v.v3.y) + ", " + double_to_string(v.v3.z);
  }
  else if (v.type == Block::ValueType::VEC4)
  {
    str += ":p4 = ";
    str += double_to_string(v.v4.x) + ", " + double_to_string(v.v4.y) + ", " + double_to_string(v.v4.z) +
           ", " + double_to_string(v.v4.w);
  }
  else if (v.type == Block::ValueType::IVEC2)
  {
    str += ":i2 = ";
    str += std::to_string(v.iv2.x) + ", " + std::to_string(v.iv2.y);
  }
  else if (v.type == Block::ValueType::IVEC3)
  {
    str += ":i3 = ";
    str += std::to_string(v.iv3.x) + ", " + std::to_string(v.iv3.y) + ", " + std::to_string(v.iv3.z);
  }
  else if (v.type == Block::ValueType::IVEC4)
  {
    str += ":i4 = ";
    str += std::to_string(v.iv4.x) + ", " + std::to_string(v.iv4.y) + ", " + std::to_string(v.iv4.z) +
           ", " + std::to_string(v.iv4.w);
  }
  else if (v.type == Block::ValueType::MAT4)
  {
    str += ":m4 = ";
    for (int i = 0; i < 4; i++)
    {
      for (int j = 0; j < 4; j++)
      {
        str += double_to_string(v.m4(i, j));
        if (i < 3 || j < 3)
          str += ", ";
        if (j == 3)
          str += "  ";
      }
    }
  }
  else if (v.type == Block::ValueType::ENUM)
  {
    str += ":e_" + std::string(get_enum_info()[v.ev.type_id].name) + " = ";
    str += get_enum_info()[v.ev.type_id].names[v.ev.val_id];
  }
  else if (v.type == Block::ValueType::STRING && v.s)
  {
    str += ":s = \"" + save_string(*(v.s)) + "\"";
  }
  else if (v.type == Block::ValueType::ARRAY && v.a)
  {
    str += ":arr = ";
    save_arr(str, *(v.a));
  }
  else if (v.type == Block::ValueType::BLOCK && v.bl)
  {
    str += " ";
    save_block(str, *(v.bl));
  }
}

void save_block_to_string(std::string &str, Block &b)
{
  save_block(str, b);
}

void save_block_to_file(std::string path, Block &b)
{
  std::string input;

  save_block(input, b);

  std::ofstream out(path);
  out << input;
  out.close();
}

void Block::Value::clear()
{
  if (type == Block::ValueType::BLOCK && bl)
  {
    bl->clear();
    delete bl;
  }
  else if (type == Block::ValueType::ARRAY && a)
    delete a;
  else if (type == Block::ValueType::STRING && s)
    delete s;

  type = Block::ValueType::EMPTY;
}
void Block::clear()
{
  for (int i = 0; i < size(); i++)
  {
    values[i].clear();
  }
  values.clear();
  names.clear();
}
bool Block::has_tag(const std::string &name) const
{
  int id = get_id(name);
  return id >= 0 && (get_type(id) == Block::ValueType::EMPTY);
}

void Block::add_bool(const std::string name, bool base_val)
{
  Block::Value val;
  val.type = Block::ValueType::BOOL;
  val.b = base_val;
  add_value(name, val);
}
void Block::add_int(const std::string name, int base_val)
{
  Block::Value val;
  val.type = Block::ValueType::INT;
  val.i = base_val;
  add_value(name, val);
}
void Block::add_uint64(const std::string name, uint64_t base_val)
{
  Block::Value val;
  val.type = Block::ValueType::UINT64;
  val.u = base_val;
  add_value(name, val);
}
void Block::add_double(const std::string name, double base_val)
{
  Block::Value val;
  val.type = Block::ValueType::DOUBLE;
  val.d = base_val;
  add_value(name, val);
}
void Block::add_vec2(const std::string name, float2 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::VEC2;
  val.v2 = base_val;
  add_value(name, val);
}
void Block::add_vec3(const std::string name, float3 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::VEC3;
  val.v3 = base_val;
  add_value(name, val);
}
void Block::add_vec4(const std::string name, float4 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::VEC4;
  val.v4 = base_val;
  add_value(name, val);
}
void Block::add_ivec2(const std::string name, int2 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::IVEC2;
  val.iv2 = base_val;
  add_value(name, val);
}
void Block::add_ivec3(const std::string name, int3 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::IVEC3;
  val.iv3 = base_val;
  add_value(name, val);
}
void Block::add_ivec4(const std::string name, int4 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::IVEC4;
  val.iv4 = base_val;
  add_value(name, val);
}
void Block::add_mat4(const std::string name, float4x4 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::MAT4;
  val.m4 = base_val;
  add_value(name, val);
}
void Block::add_enum(const std::string name, const std::string &type_name, unsigned base_val)
{
  auto info_it = get_enum_info_by_name().find(type_name);
  if (info_it == get_enum_info_by_name().end())
  {
    fprintf(stderr, "[add_enum::ERROR] enum %s is not registered\n", type_name.c_str());
    return;
  }
  auto val_it = get_enum_info()[info_it->second].id_by_val.find(base_val);
  if (val_it == get_enum_info()[info_it->second].id_by_val.end())
  {
    fprintf(stderr, "[add_enum::ERROR] enum %s has no value %u\n", type_name.c_str(), base_val);
    return;
  }
  Block::Value val;
  val.type = Block::ValueType::ENUM;
  val.ev.type_id = info_it->second;
  val.ev.val_id = val_it->second;
  add_value(name, val);
}
void Block::add_string(const std::string name, std::string base_val)
{
  Block::Value val;
  val.type = Block::ValueType::STRING;
  val.s = new std::string(base_val);
  add_value(name, val);
}
void Block::add_block(const std::string name, Block *bl)
{
  Block::Value val;
  val.type = Block::ValueType::BLOCK;

  val.bl = new Block();
  if (bl != nullptr)
    val.bl->copy(bl);
  add_value(name, val);
}
void Block::add_arr(const std::string name, std::vector<double> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (double &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  add_value(name, val);
}
void Block::add_arr(const std::string name, std::vector<float> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (float &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  add_value(name, val);
}
void Block::add_arr(const std::string name, std::vector<int> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (int &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  add_value(name, val);
}
void Block::add_arr(const std::string name, std::vector<unsigned> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (unsigned &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  add_value(name, val);
}
void Block::add_arr(const std::string name, std::vector<short> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (short &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  add_value(name, val);
}
void Block::add_arr(const std::string name, std::vector<unsigned short> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (unsigned short &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  add_value(name, val);
}
void Block::add_arr(const std::string name, std::vector<std::string> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::STRING;
  for (std::string &str : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::STRING;
    av.s = new std::string(str);
    val.a->values.push_back(av);
  }
  add_value(name, val);
}

void Block::set_bool(const std::string name, bool base_val)
{
  Block::Value val;
  val.type = Block::ValueType::BOOL;
  val.b = base_val;
  set_value(name, val);
}
void Block::set_int(const std::string name, int base_val)
{
  Block::Value val;
  val.type = Block::ValueType::INT;
  val.i = base_val;
  set_value(name, val);
}
void Block::set_uint64(const std::string name, uint64_t base_val)
{
  Block::Value val;
  val.type = Block::ValueType::UINT64;
  val.u = base_val;
  set_value(name, val);
}
void Block::set_double(const std::string name, double base_val)
{
  Block::Value val;
  val.type = Block::ValueType::DOUBLE;
  val.d = base_val;
  set_value(name, val);
}
void Block::set_vec2(const std::string name, float2 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::VEC2;
  val.v2 = base_val;
  set_value(name, val);
}
void Block::set_vec3(const std::string name, float3 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::VEC3;
  val.v3 = base_val;
  set_value(name, val);
}
void Block::set_vec4(const std::string name, float4 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::VEC4;
  val.v4 = base_val;
  set_value(name, val);
}
void Block::set_ivec2(const std::string name, int2 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::IVEC2;
  val.iv2 = base_val;
  set_value(name, val);
}
void Block::set_ivec3(const std::string name, int3 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::IVEC3;
  val.iv3 = base_val;
  set_value(name, val);
}
void Block::set_ivec4(const std::string name, int4 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::IVEC4;
  val.iv4 = base_val;
  set_value(name, val);
}
void Block::set_mat4(const std::string name, float4x4 base_val)
{
  Block::Value val;
  val.type = Block::ValueType::MAT4;
  val.m4 = base_val;
  set_value(name, val);
}
void Block::set_enum(const std::string name, const std::string &type_name, unsigned base_val)
{
  auto info_it = get_enum_info_by_name().find(type_name);
  if (info_it == get_enum_info_by_name().end())
  {
    fprintf(stderr, "[add_enum::ERROR] enum %s is not registered\n", type_name.c_str());
    return;
  }
  auto val_it = get_enum_info()[info_it->second].id_by_val.find(base_val);
  if (val_it == get_enum_info()[info_it->second].id_by_val.end())
  {
    fprintf(stderr, "[add_enum::ERROR] enum %s has no value %u\n", type_name.c_str(), base_val);
    return;
  }
  Block::Value val;
  val.type = Block::ValueType::ENUM;
  val.ev.type_id = info_it->second;
  val.ev.val_id = val_it->second;
  set_value(name, val);
}
void Block::set_string(const std::string name, std::string base_val)
{
  Block::Value val;
  val.type = Block::ValueType::STRING;
  val.s = new std::string(base_val);
  set_value(name, val);
}
void Block::set_block(const std::string name, Block *bl)
{
  Block::Value val;
  val.type = Block::ValueType::BLOCK;
  val.bl = new Block();
  val.bl->copy(bl);
  set_value(name, val);
}
void Block::set_arr(const std::string name, const std::vector<double> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (const double &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  set_value(name, val);
}
void Block::set_arr(const std::string name, const std::vector<float> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (const float &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  set_value(name, val);
}
void Block::set_arr(const std::string name, const std::vector<int> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (const int &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  set_value(name, val);
}
void Block::set_arr(const std::string name, const std::vector<unsigned> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (const unsigned &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  set_value(name, val);
}
void Block::set_arr(const std::string name, const std::vector<short> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (const short &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  set_value(name, val);
}
void Block::set_arr(const std::string name, const std::vector<unsigned short> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::DOUBLE;
  for (const unsigned short &d : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::DOUBLE;
    av.d = d;
    val.a->values.push_back(av);
  }
  set_value(name, val);
}
void Block::set_arr(const std::string name, const std::vector<std::string> &_values)
{
  Block::Value val;
  val.type = Block::ValueType::ARRAY;
  val.a = new Block::DataArray();
  val.a->type = Block::ValueType::STRING;
  for (const std::string &str : _values)
  {
    Block::Value av;
    av.type = Block::ValueType::STRING;
    av.s = new std::string(str);
    val.a->values.push_back(av);
  }
  set_value(name, val);
}
std::string Block::get_name(int id) const
{
  return (id >= 0 && id < names.size()) ? names[id] : "";
}
void Block::add_value(const std::string &name, const Block::Value &value)
{
  values.push_back(value);
  names.push_back(name);
}
void Block::set_value(const std::string &name, const Block::Value &value)
{
  int id = get_id(name);
  if (id >= 0)
  {
    values[id].copy(value);
  }
  else
    add_value(name, value);
}

void Block::add_detalization(Block &det)
{
  for (int i = 0; i < det.size(); i++)
  {
    int id = get_id(det.get_name(i));
    if (id >= 0 && values[id].type == det.get_type(i))
    {
      if (values[id].type == ValueType::BLOCK)
      {
        if (values[id].bl && det.values[i].bl)
          values[id].bl->add_detalization(*(det.values[i].bl));
      }
      else
        values[id].copy(det.values[i]);
      // fprintf(stderr, "detalization added %s %d %d",det.get_name(i).c_str(),values[id].bl, det.values[i].bl);
    }
  }
}

void Block::copy(const Block *b)
{
  names = b->names;
  values = b->values;
  for (int i = 0; i < b->names.size(); i++)
  {
    if (values[i].type == ValueType::ARRAY && values[i].a)
    {
      values[i].a = new DataArray();
      values[i].a->type = b->values[i].a->type;
      values[i].a->values = b->values[i].a->values;
    }
    if (values[i].type == ValueType::BLOCK && values[i].bl)
    {
      values[i].bl = new Block();
      values[i].bl->copy(b->values[i].bl);
    }
    if (values[i].type == ValueType::STRING && values[i].s)
    {
      values[i].s = new std::string();
      *(values[i].s) = *(b->values[i].s);
    }
  }
}

Block::~Block()
{
  clear();
}
Block &Block::operator=(Block &b)
{
  clear();
  copy(&b);
  return *this;
}
Block &Block::operator=(Block &&b)
{
  clear();
  names = std::move(b.names);
  values = std::move(b.values);
  return *this;
}