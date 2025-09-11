#include "ttf_reader.h"
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
      float x;
      float y;
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
    std::vector<uint16_t> instructions;
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
      instructions[i] = big_to_little_endian<uint16_t>(bytes + off);
      off += 2;
    }

    uint32_t cur_contour = 0;
    uint32_t cur_point_local = 0;
    uint32_t cur_point = 0;
    uint32_t flags_skip = 0;
    TTFSimpleGlyph::Point prev_point;
    for (int cur_point = 0; cur_point < endPtsOfContours.back(); cur_point++)
    {
      TTFSimpleGlyph::Point point;
      if (flags_skip == 0)
      {
        point.flags = ((TTFSimpleGlyph::Flags *)bytes)[off];
        off++;

        if (point.flags.repeat)
        {
          flags_skip = bytes[off];
          off++;
        }
      }
      else
      {
        point.flags = prev_point.flags;
        flags_skip--;
      }

      if (point.flags.x_short_vector)
      {
        uint8_t abs_x_shift = bytes[off];
        off++;
        point.x = prev_point.x + (point.flags.x_is_same ? 1.0f : -1.0f) * float(abs_x_shift);
      }
      else
      {
        uint16_t abs_x_shift = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        point.x = point.flags.x_is_same ? prev_point.x : float(abs_x_shift);
      }

      if (point.flags.y_short_vector)
      {
        uint8_t abs_y_shift = bytes[off];
        off++;
        point.y = prev_point.y + (point.flags.y_is_same ? 1.0f : -1.0f) * float(abs_y_shift);
      }
      else
      {
        uint16_t abs_y_shift = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        point.y = point.flags.y_is_same ? prev_point.y : float(abs_y_shift);
      }

      printf("point %d %f %f\n", cur_point, point.x, point.y);
      prev_point = point;
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
    return true;
  }
}