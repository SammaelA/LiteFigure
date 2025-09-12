#include "ttf_reader.h"
#include "figure.h"
#include <cstdint>
#include <vector>
#include <fstream>
#include <cstring>
namespace LiteFigure
{
  template <typename T>
  T big_to_little_endian(const uint8_t *data)
  {
    uint8_t res[sizeof(T)] = {0};
    for (int i = 0; i < sizeof(T); i++)
    {
      res[i] = data[sizeof(T) - i - 1];
    }
    return *reinterpret_cast<T*>(res);
  }

  uint32_t CalcTableChecksum(uint32_t *table, uint32_t numberOfBytesInTable)
	{
    uint32_t sum = 0;
    uint32_t nLongs = (numberOfBytesInTable + 3) / 4;
    while (nLongs-- > 0)
      sum += *table++;
    return sum;
	}

  struct TTFOffsetSubtable
  {
    uint32_t scalerType = 0;
    uint16_t numTables = 0;
    uint16_t searchRange = 0;
    uint16_t entrySelector = 0;
    uint16_t rangeShift = 0;
  };

  struct TTFTableDirectory
  {
    char tag[4];
    uint32_t checkSum = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
  };

  struct TTFHeadTable
  {
    uint32_t version;
    uint32_t fontRevision;
    uint32_t checkSumAdjustment;
    uint32_t magicNumber;
    uint32_t flags;
    uint16_t unitsPerEm;
    uint64_t created; //longDateTime
    uint64_t modified;//longDateTime
    int16_t  xMin; // in FUnits
    int16_t  yMin; // in FUnits
    int16_t  xMax; // in FUnits
    int16_t  yMax; // in FUnits
    uint16_t macStyle;
    uint16_t lowestRecPPEM;
    int16_t  fontDirectionHint;
    int16_t  indexToLocFormat;
    int16_t  glyphDataFormat;
  };

  // https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6maxp.html
  struct TTFMaxpTable
  {
    uint32_t version;
    uint16_t maxGlyphs;
    uint16_t maxPoints;
    uint16_t maxContours;
    //there are some more fields, but we do not use it in our reader
  };

  struct TTFSimpleGlyph
  {
    struct Flags
    {
      uint8_t on_curve       : 1;
      uint8_t x_short_vector : 1;
      uint8_t y_short_vector : 1;
      uint8_t repeat         : 1;
      uint8_t x_is_same      : 1;
      uint8_t y_is_same      : 1;
      uint8_t reserved       : 2;
    };
    struct Point
    {
      int16_t x;
      int16_t y;
      Flags flags;
    };
    
    struct Contour
    {
      std::vector<Point> points; 
    };
    int16_t xMin;
    int16_t yMin;
    int16_t xMax;
    int16_t yMax;
    std::vector<Contour> contours;
  };

  struct TTFGlyphSet
  {
    std::vector<TTFSimpleGlyph> simple_glyphs;
  };

  TTFSimpleGlyph read_simple_glyph(const uint8_t *bytes)
  {
    TTFSimpleGlyph glyph;
    uint32_t off = 0;

    int number_of_contours = big_to_little_endian<int16_t>(bytes + off);
    off += 2;
    glyph.xMin = big_to_little_endian<int16_t>(bytes + off);
    off += 2;
    glyph.yMin = big_to_little_endian<int16_t>(bytes + off);
    off += 2;
    glyph.xMax = big_to_little_endian<int16_t>(bytes + off);
    off += 2;
    glyph.yMax = big_to_little_endian<int16_t>(bytes + off);
    off += 2;

    printf("contours %d\n", number_of_contours);
    if (number_of_contours == 0)
    {
      //empty glyph
      return glyph;
    }
    if (number_of_contours < 0)
    {
      printf("Compound glyphs are not supported\n");
      return TTFSimpleGlyph();
    }
    glyph.contours.resize(number_of_contours);

    std::vector<uint16_t> endPtsOfContours(number_of_contours);
    std::vector<uint8_t> instructions;
    for (int i = 0; i < number_of_contours; i++)
    {
      endPtsOfContours[i] = big_to_little_endian<uint16_t>(bytes + off);
      off += 2;
      printf("%d\n", (int)endPtsOfContours[i]);
    }
    uint32_t instructions_cnt = big_to_little_endian<uint16_t>(bytes + off);
    off += 2;
    instructions.resize(instructions_cnt);
    for (int i = 0; i < instructions_cnt; i++)
    {
      instructions[i] = bytes[off];
      off++;
    }

    // filling flags
    std::vector<TTFSimpleGlyph::Flags> flags(endPtsOfContours.back() + 1);
    uint32_t flags_skip = 0;
    for (int cur_point = 0; cur_point <= endPtsOfContours.back(); cur_point++)
    {
      if (flags_skip == 0)
      {
        flags[cur_point] = ((TTFSimpleGlyph::Flags *)bytes)[off];
        off++;

        if (flags[cur_point].repeat)
        {
          flags_skip = bytes[off];
          off++;
        }
      }
      else
      {
        flags[cur_point] = flags[cur_point - 1];
        flags_skip--;
      }
      printf("flags[%d] = %d %d %d %d %d %d\n", cur_point,
        (int)flags[cur_point].on_curve,
        (int)flags[cur_point].x_short_vector,
        (int)flags[cur_point].y_short_vector,
        (int)flags[cur_point].repeat,
        (int)flags[cur_point].x_is_same,
        (int)flags[cur_point].y_is_same);
    }

    //filling contours, no points here yet
    uint32_t cur_contour = 0;
    uint32_t cur_point_local = 0;
    for (uint32_t cur_point = 0; cur_point <= endPtsOfContours.back(); cur_point++)
    {
      if (cur_point > endPtsOfContours[cur_contour])
      {
        cur_contour++;
        cur_point_local = 0;
      }
      glyph.contours[cur_contour].points.push_back(TTFSimpleGlyph::Point());
      glyph.contours[cur_contour].points[cur_point_local].flags = flags[cur_point];
      cur_point_local++;
    }

    //filling point x coordinates
    cur_contour = 0;
    cur_point_local = 0;
    int16_t coord_val = 0;
    for (int cur_point = 0; cur_point <= endPtsOfContours.back(); cur_point++)
    {
      int16_t abs_x_shift = 0;
      TTFSimpleGlyph::Point &point = glyph.contours[cur_contour].points[cur_point_local];
      if (point.flags.x_short_vector)
      {
        abs_x_shift = int16_t(bytes[off]);
        off++;
        coord_val += (point.flags.x_is_same ? 1 : -1) * int16_t(abs_x_shift);
      }
      else if (!point.flags.x_is_same)
      {
        abs_x_shift = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        coord_val += abs_x_shift;
      }
      point.x = coord_val;

      cur_point_local++;
      if (cur_point == endPtsOfContours[cur_contour])
      {
        cur_contour++;
        cur_point_local = 0;
      }
    }

    //filling point y coordinates
    cur_contour = 0;
    cur_point_local = 0;
    coord_val = 0;
    for (int cur_point = 0; cur_point <= endPtsOfContours.back(); cur_point++)
    {
      int16_t abs_y_shift = 0;
      TTFSimpleGlyph::Point &point = glyph.contours[cur_contour].points[cur_point_local];
      if (point.flags.y_short_vector)
      {
        abs_y_shift = int16_t(bytes[off]);
        off++;
        coord_val += (point.flags.y_is_same ? 1 : -1) * int16_t(abs_y_shift);
      }
      else if (!point.flags.y_is_same)
      {
        abs_y_shift = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        coord_val += abs_y_shift;
      }
      point.y = coord_val;  

      cur_point_local++;
      if (cur_point == endPtsOfContours[cur_contour])
      {
        cur_contour++;
        cur_point_local = 0;
      }
    }

    //print points
    for (auto &c : glyph.contours)
    {
      for (auto &p : c.points)
      {
        printf("(%d,%d)%s ", (int)p.x, (int)p.y, p.flags.on_curve ? "o" : "x");
      }
      printf("\n");
    }

    return glyph;
  }

  TTFHeadTable read_head_table(const uint8_t *bytes)
  {
    uint32_t off = 0;
    TTFHeadTable table;
    table.version            = big_to_little_endian<uint32_t>(bytes + off); off += 4;
    table.fontRevision       = big_to_little_endian<uint32_t>(bytes + off); off += 4;
    table.checkSumAdjustment = big_to_little_endian<uint32_t>(bytes + off); off += 4;
    table.magicNumber        = big_to_little_endian<uint32_t>(bytes + off); off += 4;
    table.flags              = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    table.unitsPerEm         = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    table.created            = big_to_little_endian<uint64_t>(bytes + off); off += 8;
    table.modified           = big_to_little_endian<uint64_t>(bytes + off); off += 8;
    table.xMin               = big_to_little_endian< int16_t>(bytes + off); off += 2;
    table.yMin               = big_to_little_endian< int16_t>(bytes + off); off += 2;
    table.xMax               = big_to_little_endian< int16_t>(bytes + off); off += 2;
    table.yMax               = big_to_little_endian< int16_t>(bytes + off); off += 2;
    table.macStyle           = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    table.lowestRecPPEM      = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    table.fontDirectionHint  = big_to_little_endian< int16_t>(bytes + off); off += 2;
    table.indexToLocFormat   = big_to_little_endian< int16_t>(bytes + off); off += 2;
    table.glyphDataFormat    = big_to_little_endian< int16_t>(bytes + off); off += 2;
    printf("%08x %d %d %d %d indexToLoc %d\n",
      table.magicNumber, table.xMin, table.yMin, table.xMax, table.yMax, table.indexToLocFormat);
    return table;
  }

  TTFMaxpTable read_maxp_table(const uint8_t *bytes)
  {
    uint32_t off = 0;
    TTFMaxpTable table;
    table.version   = big_to_little_endian<uint32_t>(bytes + off); off += 4;
    table.maxGlyphs = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    table.maxPoints = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    table.maxContours = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    printf("maxp version %08x, maxGlyphs %d, maxPoints %d, maxContours %d\n",
      table.version, table.maxGlyphs, table.maxPoints, table.maxContours);
    return table;
  }

  std::vector<uint32_t> get_all_glyph_locations(const uint8_t *bytes, uint32_t numGlyphs,
                                                uint32_t bytesPerEntry, uint32_t locaTableLocation)
  {
    std::vector<uint32_t> all_glyph_locations(numGlyphs);
    bool isTwoByteEntry = bytesPerEntry == 2;
    for (int glyphIndex = 0; glyphIndex < numGlyphs; glyphIndex++)
    {
      if (isTwoByteEntry)
        all_glyph_locations[glyphIndex] = big_to_little_endian<uint16_t>(bytes + locaTableLocation + glyphIndex * bytesPerEntry) * 2u;
      else
        all_glyph_locations[glyphIndex] = big_to_little_endian<uint32_t>(bytes + locaTableLocation + glyphIndex * bytesPerEntry);
      printf("glyph %d offset %d\n", glyphIndex, all_glyph_locations[glyphIndex]);
    }
    return all_glyph_locations;
  }
  
  void debug_render_glyph_table(const std::vector<TTFSimpleGlyph> &all_glyphs)
  {
    std::shared_ptr<Grid> grid = std::make_shared<Grid>();
    int glyphs_per_row = 16;
    int rows = (all_glyphs.size() + glyphs_per_row - 1) / glyphs_per_row;
    float glyph_scale = 0.5f;

    grid->rows.resize(rows);
    float4 colors[4] = {float4(1,1,1,1), float4(1,1,0,1), float4(1,0,1,1), float4(0,1,1,1)};
    for (int gId = 0; gId < all_glyphs.size(); gId++)
    {
      const TTFSimpleGlyph &glyph = all_glyphs[gId];
      int row = gId / glyphs_per_row;
      int col = gId % glyphs_per_row;
      float2 sz = float2(1.1f*std::max(glyph.xMax - glyph.xMin, glyph.yMax - glyph.yMin));

      if (glyph.contours.size() == 0)
      {
        std::shared_ptr<PrimitiveFill> fill = std::make_shared<PrimitiveFill>();
        fill->color = float4(1,0,1,1);
        fill->size = int2(glyph_scale*sz);
        grid->rows[row].push_back(fill);
        continue;
      }

      std::shared_ptr<Collage> collage = std::make_shared<Collage>();
      collage->size = int2(glyph_scale*sz);
      printf("glyph %d size %d,%d\n", gId, (int)collage->size.x, (int)collage->size.y);
      
      for (int cId = 0; cId < glyph.contours.size(); cId++)
      {
        const TTFSimpleGlyph::Contour &contour = glyph.contours[cId];
        for (int pId = 0; pId < contour.points.size(); pId++)
        {
          const TTFSimpleGlyph::Point &p0 = contour.points[pId];
          const TTFSimpleGlyph::Point &p1 = contour.points[(pId + 1) % contour.points.size()];
          float2 p0f = float2(float(p0.x - glyph.xMin), float(p0.y - glyph.yMin)) / sz;
          float2 p1f = float2(float(p1.x - glyph.xMin), float(p1.y - glyph.yMin)) / sz;
          std::shared_ptr<Line> line = std::make_shared<Line>();
          line->start = p0f;
          line->end = p1f;
          line->thickness = 0.01f;
          line->color = colors[cId % 4];
          collage->elements.emplace_back();
          collage->elements.back().figure = line;
          collage->elements.back().pos = int2(0,0);
          collage->elements.back().size = collage->size;

          std::shared_ptr<Circle> circle = std::make_shared<Circle>();
          circle->center = p0f;
          circle->radius = 0.01f;
          circle->color = p0.flags.on_curve ? float4(1,0,0,1) : float4(0,0,1,1);
          collage->elements.emplace_back();
          collage->elements.back().figure = circle;
          collage->elements.back().pos = int2(0,0);
          collage->elements.back().size = collage->size;
        }
      }
      grid->rows[row].push_back(collage);
    }

    auto image = render_figure_to_image(grid);
    LiteImage::SaveImage("saves/glyphs.png", image);
  }

  bool read_ttf_debug(const std::string &filename)
  {
    std::vector<uint8_t> buffer;
    TTFOffsetSubtable offsetSubtable;
    std::vector<TTFTableDirectory> table_directories;

    buffer.reserve(100000); //should be enough for most fonts
    std::ifstream file(filename, std::ios::binary);
    file.seekg(0, std::ios::end);
    buffer.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    file.close();

    printf("read %d bytes\n", (int)buffer.size());
    offsetSubtable.scalerType = big_to_little_endian<uint32_t>(buffer.data());
    offsetSubtable.numTables = big_to_little_endian<uint16_t>(buffer.data() + 4);
    offsetSubtable.searchRange = big_to_little_endian<uint16_t>(buffer.data() + 6);
    offsetSubtable.entrySelector = big_to_little_endian<uint16_t>(buffer.data() + 8);
    offsetSubtable.rangeShift = big_to_little_endian<uint16_t>(buffer.data() + 10);
    printf("scalerType   : 0x%08x\n", offsetSubtable.scalerType);
    printf("numTables    : %d\n", offsetSubtable.numTables);
    printf("searchRange  : %d\n", offsetSubtable.searchRange);
    printf("entrySelector: %d\n", offsetSubtable.entrySelector);
    printf("rangeShift   : %d\n", offsetSubtable.rangeShift);

    table_directories.resize(offsetSubtable.numTables);
    uint32_t cur_off = 12;
    for (int i = 0; i < offsetSubtable.numTables; i++)
    {
      TTFTableDirectory &table = table_directories[i];
      for (int j=0;j<4;j++)
      table.tag[j] = buffer[cur_off+j];
      table.checkSum = big_to_little_endian<uint32_t>(buffer.data() + cur_off + 4);
      table.offset = big_to_little_endian<uint32_t>(buffer.data() + cur_off + 8);
      table.length = big_to_little_endian<uint32_t>(buffer.data() + cur_off + 12);
      printf("table %02d: tag \'%c%c%c%c\', offset %5d, length %5d\n", i, 
             table.tag[0], table.tag[1], table.tag[2], table.tag[3], 
             table.offset, table.length);
      cur_off += 16;
    }

    uint32_t head_table_id = (uint32_t)-1;
    uint32_t glyph_table_id = (uint32_t)-1;
    uint32_t maxp_table_id = (uint32_t)-1;
    uint32_t loca_table_id = (uint32_t)-1;
    for (int i=0;i<offsetSubtable.numTables;i++)
    {
      if (strncmp("glyf", table_directories[i].tag, 4) == 0)
        glyph_table_id = i;
      else if (strncmp("head", table_directories[i].tag, 4) == 0)
        head_table_id = i;
      else if (strncmp("maxp", table_directories[i].tag, 4) == 0)
        maxp_table_id = i;
      else if (strncmp("loca", table_directories[i].tag, 4) == 0)
        loca_table_id = i;
    }

    if (head_table_id == (uint32_t)-1)
    {
      printf("Error: no head table found\n");
      return false;
    }
    if (maxp_table_id == (uint32_t)-1)
    {
      printf("Error: no maxp table found\n");
      return false;
    }
    if (loca_table_id == (uint32_t)-1)
    {
      printf("Error: no loca table found\n");
      return false;
    }
    if (glyph_table_id == (uint32_t)-1)
    {
      printf("Error: no glyf table found\n");
      return false;
    }

    TTFHeadTable headTable = read_head_table(buffer.data() + table_directories[head_table_id].offset);
    TTFMaxpTable maxpTable = read_maxp_table(buffer.data() + table_directories[maxp_table_id].offset);
    std::vector<uint32_t> all_glyph_locations = get_all_glyph_locations(
      buffer.data(), maxpTable.maxGlyphs, 
      headTable.indexToLocFormat == 0 ? 2 : 4,
      table_directories[loca_table_id].offset);

    std::vector<TTFSimpleGlyph> all_glyphs;
    for (uint32_t glyphId = 0; glyphId < maxpTable.maxGlyphs; glyphId++)
    {
      uint32_t off = table_directories[glyph_table_id].offset + all_glyph_locations[glyphId];
      printf("read glyph %d/%d at offset %d\n", glyphId, maxpTable.maxGlyphs, off);
      all_glyphs.push_back(read_simple_glyph(buffer.data() + off));
    }

    //debug_render_glyph_table(all_glyphs);

    return true;
  }
}