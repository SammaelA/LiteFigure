#include "ttf_reader.h"
#include "figure.h"
#include "font.h"
#include <cstdint>
#include <vector>
#include <fstream>
#include <cstring>
#include <map>

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

  struct TTFHorizontalHeaderTable
  {
    uint32_t version;
    int16_t  ascent;
    int16_t  descent;
    int16_t  lineGap;
    uint16_t advanceWidthMax;
    int16_t  minLeftSideBearing;
    int16_t  minRightSideBearing;
    int16_t  xMaxExtent;
    int16_t  caretSlopeRise;
    int16_t  caretSlopeRun;
    int16_t  caretOffset;
    int16_t  reserved1;
    int16_t  reserved2;
    int16_t  reserved3;
    int16_t  reserved4;
    int16_t  metricDataFormat;
    uint16_t numOfLongHorMetrics;
  };

  struct TTFCompoundGlyph
  {
    struct Flags
    {
      uint8_t ARG_1_AND_2_ARE_WORDS    : 1;
      uint8_t ARGS_ARE_XY_VALUES       : 1;
      uint8_t ROUND_XY_TO_GRID         : 1;
      uint8_t WE_HAVE_A_SCALE          : 1;
      uint8_t _pad                     : 1;
      uint8_t MORE_COMPONENTS          : 1;
      uint8_t WE_HAVE_AN_X_AND_Y_SCALE : 1;
      uint8_t WE_HAVE_A_TWO_BY_TWO     : 1;
      uint8_t WE_HAVE_INSTRUCTIONS     : 1;
      uint8_t USE_MY_METRICS           : 1;
      uint8_t OVERLAP_COMPOUND         : 1;
    };
    struct Component
    {
      uint32_t glyphIndex; //index to glyph in glyf table
      float a,b,c,d; // transformation 
      int16_t offsetX, offsetY; // translation (e,f from manual)
    };
    
    int16_t xMin;
    int16_t yMin;
    int16_t xMax;
    int16_t yMax;   
    std::vector<Component> components;
  };

  struct TTFGlyphSet
  {
    std::vector<TTFSimpleGlyph> simple_glyphs;
    std::vector<TTFCompoundGlyph> compound_glyphs;
    std::vector<uint32_t> glyph_locations; //for each glyph, its position in simple_glyphs or compound_glyphs
    std::vector<bool> is_compound; //for each glyph, true if compound, false if simple
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
      // printf("flags[%d] = %d %d %d %d %d %d\n", cur_point,
      //   (int)flags[cur_point].on_curve,
      //   (int)flags[cur_point].x_short_vector,
      //   (int)flags[cur_point].y_short_vector,
      //   (int)flags[cur_point].repeat,
      //   (int)flags[cur_point].x_is_same,
      //   (int)flags[cur_point].y_is_same);
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

    return glyph;
  }

  float FixedPoint2Dot14ToFloat(int16_t val)
  {
    return float(val) / float(1 << 14);
  }

  TTFCompoundGlyph read_compound_glyph(const uint8_t *bytes)
  {
    TTFCompoundGlyph glyph;
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

    if (number_of_contours >= 0)
    {
      printf("Simple glyphs are not supported in read_compound_glyph\n");
      return TTFCompoundGlyph();
    }

    bool more_components = true;
    while (more_components)
    {
      uint16_t flags_raw = big_to_little_endian<uint16_t>(bytes + off);
      off += 2;
      TTFCompoundGlyph::Flags flags = *(TTFCompoundGlyph::Flags*)&flags_raw;
      more_components = flags.MORE_COMPONENTS;

      TTFCompoundGlyph::Component component;
      component.glyphIndex = big_to_little_endian<uint16_t>(bytes + off);
      off += 2;

      if (flags.ARG_1_AND_2_ARE_WORDS)
      {
        //arguments are int16
        int16_t arg1 = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        int16_t arg2 = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        if (flags.ARGS_ARE_XY_VALUES)
        {
          component.offsetX = arg1;
          component.offsetY = arg2;
        }
        else
        {
          printf("Warning: we do not support matching by point numbers\n");
        }
      }
      else
      {
        //arguments are uint8
        int8_t arg1 = bytes[off];
        off++;
        int8_t arg2 = bytes[off];
        off++;
        if (flags.ARGS_ARE_XY_VALUES)
        {
          component.offsetX = arg1;
          component.offsetY = arg2;
        }
        else
        {
          printf("Warning: we do not support matching by point numbers\n");
        }
      }

      if (flags.WE_HAVE_A_SCALE)
      {
        int16_t scale_fixed = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        float scale = FixedPoint2Dot14ToFloat(scale_fixed);
        component.a = scale;
        component.b = 0;
        component.c = 0;
        component.d = scale;
      }
      else if (flags.WE_HAVE_AN_X_AND_Y_SCALE)
      {
        int16_t xscale_fixed = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        int16_t yscale_fixed = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        float xscale = FixedPoint2Dot14ToFloat(xscale_fixed);
        float yscale = FixedPoint2Dot14ToFloat(yscale_fixed);
        component.a = xscale;
        component.b = 0;
        component.c = 0;
        component.d = yscale;
      }
      else if (flags.WE_HAVE_A_TWO_BY_TWO)
      {
        int16_t a_fixed = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        int16_t b_fixed = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        int16_t c_fixed = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        int16_t d_fixed = big_to_little_endian<int16_t>(bytes + off);
        off += 2;
        component.a = FixedPoint2Dot14ToFloat(a_fixed);
        component.b = FixedPoint2Dot14ToFloat(b_fixed);
        component.c = FixedPoint2Dot14ToFloat(c_fixed);
        component.d = FixedPoint2Dot14ToFloat(d_fixed);
      }
      else
      {
        //no scale, identity matrix
        component.a = 1.0f;
        component.b = 0.0f;
        component.c = 0.0f;
        component.d = 1.0f;
      }
      glyph.components.push_back(component);
    }
    return glyph;
  }

  TTFSimpleGlyph simplify_compound_glyph(const TTFCompoundGlyph &compound,
                                         const TTFGlyphSet &glyphSet,
                                         int recursion_depth = 0)
  {
    if (recursion_depth > 64)
    {
      printf("Error: too deep recursion in simplify_compound_glyph\n");
      return TTFSimpleGlyph();
    }
    TTFSimpleGlyph result;
    result.xMin = compound.xMin;
    result.yMin = compound.yMin;
    result.xMax = compound.xMax;
    result.yMax = compound.yMax;

    for (int cId = 0; cId < compound.components.size(); cId++)
    {
      const TTFCompoundGlyph::Component &component = compound.components[cId];
      TTFSimpleGlyph sub_glyph;
      if (glyphSet.is_compound[component.glyphIndex])
      {
        sub_glyph = simplify_compound_glyph(
          glyphSet.compound_glyphs[glyphSet.glyph_locations[component.glyphIndex]],
          glyphSet, recursion_depth + 1);
      }
      else
      {
        sub_glyph = glyphSet.simple_glyphs[glyphSet.glyph_locations[component.glyphIndex]];
      }

      //transform and add to result
      for (int contId = 0; contId < sub_glyph.contours.size(); contId++)
      {
        TTFSimpleGlyph::Contour new_contour;
        for (int pId = 0; pId < sub_glyph.contours[contId].points.size(); pId++)
        {
          const TTFSimpleGlyph::Point &p0 = sub_glyph.contours[contId].points[pId];
          TTFSimpleGlyph::Point p1;
          p1.flags = p0.flags;
          p1.x = int16_t(component.a * float(p0.x) + component.c * float(p0.y) + float(component.offsetX));
          p1.y = int16_t(component.b * float(p0.x) + component.d * float(p0.y) + float(component.offsetY));
          new_contour.points.push_back(p1);
        }
        result.contours.push_back(new_contour);
      }
    }

    return result;
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

  // Format 0 is suitable for fonts whose character codes and glyph indices are restricted to single bytes. 
  // This was a very common situation when TrueType was introduced but is rarely encountered now.
  TTFCmapTable read_cmap_table_format_0(const uint8_t *bytes)
  {
    TTFCmapTable table;

    uint32_t off = 0;
    uint16_t format = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    uint16_t length = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    uint16_t language = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    for (int i = 0; i < 256; i++)
    {
      table.charGlyphs[i] = bytes[off];
      off++;
      if (table.charGlyphs[i] != 0)
        table.unicodeToGlyph[i] = table.charGlyphs[i];
    }

    return table;
  }

  // Format 4 is a two-byte encoding format. It should be used when the character codes for a font 
  // fall into several contiguous ranges, possibly with holes in some or all of the ranges. 
  // That is, some of the codes in a range may not be associated with glyphs in the font. 
  TTFCmapTable read_cmap_table_format_4(const uint8_t *bytes)
  {
    TTFCmapTable table;
    
    uint32_t off = 0;
    uint16_t format = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    uint16_t length = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    uint16_t language = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    uint16_t segCountX2 = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    uint16_t searchRange = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    uint16_t entrySelector = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    uint16_t rangeShift = big_to_little_endian<uint16_t>(bytes + off); off += 2;

    uint16_t segCount = segCountX2 / 2;
    std::vector<uint16_t> endCode(segCount);
    std::vector<uint16_t> startCode(segCount);
    std::vector<uint16_t> idDelta(segCount);
    std::vector<uint16_t> idRangeOffset(segCount);

    for (int i = 0; i < segCount; i++)
    {
      endCode[i] = big_to_little_endian<uint16_t>(bytes + off);
      off += 2;
    }
    uint16_t reservedPad = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    assert(reservedPad == 0);

    for (int i = 0; i < segCount; i++)
    {
      startCode[i] = big_to_little_endian<uint16_t>(bytes + off);
      off += 2;
    }

    for (int i = 0; i < segCount; i++)
    {
      idDelta[i] = big_to_little_endian<uint16_t>(bytes + off);
      off += 2;
    }

    uint32_t idRangeOffset_start = off;
    for (int i = 0; i < segCount; i++)
    {
      idRangeOffset[i] = big_to_little_endian<uint16_t>(bytes + off);
      off += 2;
    }

    uint32_t glyphIdArray_start = off;
    for (int segId = 0; segId < segCount; segId++)
    {
      for (uint32_t c = startCode[segId]; c <= endCode[segId]; c++)
      {
        uint16_t glyphIndex = 0;
        if (idRangeOffset[segId] == 0)
        {
          glyphIndex = (c + idDelta[segId]) % 65536;
        }
        else
        {
          uint32_t offset_in_idRangeOffset = idRangeOffset[segId] + 2 * (c - startCode[segId]);
          uint32_t glyphIndex_address = idRangeOffset_start + 2 * segId + offset_in_idRangeOffset;
          if (glyphIndex_address >= length)
          {
            printf("Error: glyphIndex_address %d is out of range\n", glyphIndex_address);
            continue;
          }
          glyphIndex = big_to_little_endian<uint16_t>(bytes + glyphIndex_address);
          if (glyphIndex != 0)
            glyphIndex = (glyphIndex + idDelta[segId]) % 65536;
        }
        if (c < 256)
          table.charGlyphs[c] = glyphIndex;
        table.unicodeToGlyph[c] = glyphIndex;
      }
    }

    return table;
  }

  // Format 12 is a bit like format 4, in that it defines segments for sparse representation 
  // in 4-byte character space.
  TTFCmapTable read_cmap_table_format_12(const uint8_t *bytes)
  {
    TTFCmapTable table;
    return table;
  }

  TTFCmapTable read_cmap_table(const uint8_t *bytes)
  {
    constexpr uint16_t PLATFORM_ID_UNICODE = 0;
    constexpr uint16_t PLATFORM_ID_MAC     = 1;
    constexpr uint16_t PLATFORM_ID_WINDOWS = 3;

    struct SubtableHeader
    {
      uint16_t platformID;
      uint16_t encodingID;
      uint32_t offset; //from beginning of cmap table
    };

    uint32_t off = 0;
    uint16_t version = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    uint16_t numberSubtables = big_to_little_endian<uint16_t>(bytes + off); off += 2;

    printf("cmap version %d, numberSubtables %d\n", version, numberSubtables);
    std::vector<SubtableHeader> subtables(numberSubtables);
    for (int i = 0; i < numberSubtables; i++)
    {
      subtables[i].platformID = big_to_little_endian<uint16_t>(bytes + off); off += 2;
      subtables[i].encodingID = big_to_little_endian<uint16_t>(bytes + off); off += 2;
      subtables[i].offset     = big_to_little_endian<uint32_t>(bytes + off); off += 4;
      printf(" subtable %d: platformID %d, encodingID %d, offset %d\n",
        i, subtables[i].platformID, subtables[i].encodingID, subtables[i].offset);
    }

    //check encoding type to find better match for us

    int format0_index = -1;
    int format4_index = -1;
    int format12_index = -1;
    for (const auto &subtable : subtables)
    {
      uint16_t format = big_to_little_endian<uint16_t>(bytes + subtable.offset);
      if (format == 0) format0_index = subtable.offset; // ASCII should be the same for all platforms
      if (subtable.platformID == PLATFORM_ID_UNICODE)
      {
        if (format == 4) format4_index = subtable.offset;
        if (format == 12) format12_index = subtable.offset;
      }
      printf("  subtable format %d\n", format);
    }

    
    TTFCmapTable table;

    //if (format12_index >= 0)// format 12 is preferred, as it supports full 4-byte unicode
    //  table = read_cmap_table_format_12(bytes + format12_index);
    if (format4_index >= 0)// format 4 is next preferred, as it supports 2-byte unicode (aka wchar_t)
     table = read_cmap_table_format_4(bytes + format4_index);
    else if (format0_index >= 0)// format 0 is last resort, as it supports only 1-byte unicode
      table = read_cmap_table_format_0(bytes + format0_index);
    else
      printf("Error: no suitable Unicode cmap subtable found\n");
    
    return table;
  }

  TTFHorizontalHeaderTable read_hhea_table(const uint8_t *bytes)
  {
    TTFHorizontalHeaderTable table;
    uint32_t off = 0;
    table.version = big_to_little_endian<uint32_t>(bytes + off); off += 4;
    table.ascent = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.descent = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.lineGap = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.advanceWidthMax = big_to_little_endian<uint16_t>(bytes + off); off += 2;
    table.minLeftSideBearing = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.minRightSideBearing = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.xMaxExtent = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.caretSlopeRise = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.caretSlopeRun = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.caretOffset = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.reserved1 = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.reserved2 = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.reserved3 = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.reserved4 = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.metricDataFormat = big_to_little_endian<int16_t>(bytes + off); off += 2;
    table.numOfLongHorMetrics = big_to_little_endian<uint16_t>(bytes + off); off += 2;

    printf("hhea version %08x, ascent %d, descent %d, lineGap %d, numOfLongHorMetrics %d\n",
      table.version, table.ascent, table.descent, table.lineGap, table.numOfLongHorMetrics);
    return table;
  }

  std::vector<TTFLongHorMetric> read_hmtx_table(const uint8_t *bytes, uint32_t numOfLongHorMetrics, uint32_t numGlyphs)
  {
    std::vector<TTFLongHorMetric> metrics(numGlyphs);
    uint32_t off = 0;
    for (int i = 0; i < numOfLongHorMetrics; i++)
    {
      metrics[i].advanceWidth = big_to_little_endian<uint16_t>(bytes + off); off += 2;
      metrics[i].leftSideBearing = big_to_little_endian<int16_t>(bytes + off); off += 2;
      //printf("hmtx[%d] aw %d lsb %d\n", i, metrics[i].advanceWidth, metrics[i].leftSideBearing);
    }
    for (int i = numOfLongHorMetrics; i < numGlyphs; i++)
    {
      metrics[i].advanceWidth = metrics[numOfLongHorMetrics - 1].advanceWidth;
      metrics[i].leftSideBearing = big_to_little_endian<int16_t>(bytes + off); off += 2;
      //printf("hmtx[%d] aw %d lsb %d\n", i, metrics[i].advanceWidth, metrics[i].leftSideBearing);
    }
    return metrics;
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
      //printf("glyph %d offset %d\n", glyphIndex, all_glyph_locations[glyphIndex]);
    }
    return all_glyph_locations;
  }

  // Fix glyphs that represent space characters to have no contours and proper advance width.
  // This is needed because some fonts have non-empty glyphs for space characters (idk why).
  // here are the rules: https://developer.apple.com/fonts/TrueType-Reference-Manual/RM07/appendixB.html
  void fix_space_glyphs(Font &font)
  {
    unsigned char space_chars[4] = {0x09/*tab*/, 0x0D/*carriage return*/, 0x20/*space*/, 0xA0/*no-break space*/};

    // we have proper empty glyph for space, use it for all space characters
    int space_glyph_id = -1;
    if (font.glyphs[font.cmap.charGlyphs[0x20]].contours.size() == 0)
    {
      space_glyph_id = font.cmap.charGlyphs[0x20];
    }
    else
    {
      space_glyph_id = font.glyphs.size();

      TTFSimpleGlyph empty_glyph;
      empty_glyph.xMin = 0;
      empty_glyph.yMin = 0;
      empty_glyph.xMax = 1;
      empty_glyph.yMax = 1;

      //I don't know what advance width to use, so let's use advance from '0' character
      empty_glyph.advance = font.glyphs[font.cmap.charGlyphs['0']].advance;
      font.glyphs.push_back(empty_glyph);
    }

    for (char c : space_chars)
    {
      font.cmap.charGlyphs[c] = space_glyph_id;
      font.cmap.unicodeToGlyph[c] = space_glyph_id;
    }
  }

  // Add implied on-curve points between consecutive off-curve points in each contour.
  // This simplifies rendering logic, as we don't need to handle implied points on the fly.
  // Also add off-curve points between on-curve points to make all segments quadratic Beziers.
  // It is not strictly necessary, but it makes rendering logic simpler.
  void add_implied_points(TTFSimpleGlyph &glyph)
  {
    for (int cId = 0; cId < glyph.contours.size(); cId++)
    {
      TTFSimpleGlyph::Contour &contour = glyph.contours[cId];
      std::vector<TTFSimpleGlyph::Point> new_points;
      int num_points = contour.points.size();
      for (int pId = 0; pId < num_points; pId++)
      {
        const TTFSimpleGlyph::Point &p0 = contour.points[pId];
        const TTFSimpleGlyph::Point &p1 = contour.points[(pId + 1) % num_points];
        new_points.push_back(p0);
        if (!p0.flags.on_curve && !p1.flags.on_curve)
        {
          //add implied on-curve point
          TTFSimpleGlyph::Point p_implied;
          p_implied.flags = p0.flags;
          p_implied.flags.on_curve = 1 - p0.flags.on_curve;
          p_implied.x = (p0.x + p1.x) / 2;
          p_implied.y = (p0.y + p1.y) / 2;
          new_points.push_back(p_implied);
        }
      }
      contour.points = new_points;
    }
  }
  
  void debug_render_glyph_table(const Font &font, std::vector<uint32_t> glyph_ids)
  {
    const std::vector<TTFSimpleGlyph> &all_glyphs = font.glyphs;
    std::shared_ptr<Grid> grid = std::make_shared<Grid>();
    int glyphs_per_row = 16;
    int rows = (glyph_ids.size() + glyphs_per_row - 1) / glyphs_per_row;
    float glyph_scale = 256 * font.scale;

    float4 colors[4] = {float4(1,1,1,1), float4(1,1,0,1), float4(1,0,1,1), float4(0,1,1,1)};
    uint32_t curId = 0;
    for (uint32_t gId : glyph_ids)
    {
      const TTFSimpleGlyph &glyph = all_glyphs[gId];
      printf("glyph bbox: xMin %d, yMin %d, xMax %d, yMax %d, advance %d\n",
             glyph.xMin, glyph.yMin, glyph.xMax, glyph.yMax, glyph.advance.advanceWidth);
      int row = curId / glyphs_per_row;
      int col = curId % glyphs_per_row;
      if (grid->rows.size() <= row)
        grid->rows.push_back(std::vector<FigurePtr>());
      float2 sz = float2(glyph.advance.advanceWidth, 1.1f*(glyph.yMax - glyph.yMin));
      curId++;

      std::shared_ptr<Collage> collage = std::make_shared<Collage>();
      collage->size = int2(glyph_scale*sz) + int2(1,1);

      for (int cId = 0; cId < glyph.contours.size(); cId++)
      {
        const TTFSimpleGlyph::Contour &contour = glyph.contours[cId];
        for (int pId = 0; pId < contour.points.size(); pId++)
        {
          const TTFSimpleGlyph::Point &p0 = contour.points[pId];
          const TTFSimpleGlyph::Point &p1 = contour.points[(pId + 1) % contour.points.size()];
          float2 p0f = float2(float(p0.x - glyph.xMin), float(p0.y - glyph.yMin)) / sz;
          float2 p1f = float2(float(p1.x - glyph.xMin), float(p1.y - glyph.yMin)) / sz;

          //flip y axis for correct display
          p0f.y = 1.0f - p0f.y;
          p1f.y = 1.0f - p1f.y;

          std::shared_ptr<Line> line = std::make_shared<Line>();
          line->start = p0f;
          line->end = p1f;
          line->thickness = 0.03f;
          line->color = colors[cId % 4];
          collage->elements.emplace_back();
          collage->elements.back().figure = line;
          collage->elements.back().pos = int2(0,0);
          collage->elements.back().size = collage->size;

          std::shared_ptr<Circle> circle = std::make_shared<Circle>();
          circle->center = p0f;
          circle->radius = 0.03f;
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

  void debug_render_text(const Font &font, std::vector<uint32_t> glyph_ids)
  {
    const std::vector<TTFSimpleGlyph> &all_glyphs = font.glyphs;
    std::shared_ptr<Collage> top_collage = std::make_shared<Collage>();
    int max_w = 4000;
    float glyph_scale = 256 * font.scale;

    int cur_x = 0;
    int cur_y = 0;

    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;

    float4 colors[4] = {float4(1,1,1,1), float4(1,1,0,1), float4(1,0,1,1), float4(0,1,1,1)};
    for (uint32_t gId : glyph_ids)
    {
      const TTFSimpleGlyph &glyph = all_glyphs[gId];
      float2 sz = float2(glyph.xMax - glyph.xMin, glyph.yMax - glyph.yMin);
      int cur_min_x = int(glyph_scale * float(glyph.xMin));
      int cur_min_y = int(glyph_scale * float(glyph.yMin));

      min_x = std::min(min_x, cur_x + int(glyph_scale * float(glyph.xMin)));
      min_y = std::min(min_y, cur_y + int(glyph_scale * float(glyph.yMin)));
      max_x = std::max(max_x, cur_x + int(glyph_scale * float(glyph.xMax)));
      max_y = std::max(max_y, cur_y + int(glyph_scale * float(glyph.yMax)));

      std::shared_ptr<Collage> collage = std::make_shared<Collage>();
      collage->size = int2(glyph_scale*sz) + int2(1,1);

      for (int cId = 0; cId < glyph.contours.size(); cId++)
      {
        const TTFSimpleGlyph::Contour &contour = glyph.contours[cId];
        for (int pId = 0; pId < contour.points.size(); pId++)
        {
          const TTFSimpleGlyph::Point &p0 = contour.points[pId];
          const TTFSimpleGlyph::Point &p1 = contour.points[(pId + 1) % contour.points.size()];
          float2 p0f = float2(float(p0.x - glyph.xMin), float(p0.y - glyph.yMin)) / sz;
          float2 p1f = float2(float(p1.x - glyph.xMin), float(p1.y - glyph.yMin)) / sz;

          // flip y axis for correct display
          p0f.y = 1.0f - p0f.y;
          p1f.y = 1.0f - p1f.y;

          std::shared_ptr<Line> line = std::make_shared<Line>();
          line->start = p0f;
          line->end = p1f;
          line->thickness = 0.03f;
          line->color = colors[cId % 4];
          collage->elements.emplace_back();
          collage->elements.back().figure = line;
          collage->elements.back().pos = int2(0,0);
          collage->elements.back().size = collage->size;

          std::shared_ptr<Circle> circle = std::make_shared<Circle>();
          circle->center = p0f;
          circle->radius = 0.03f;
          circle->color = p0.flags.on_curve ? float4(1,0,0,1) : float4(0,0,1,1);
          collage->elements.emplace_back();
          collage->elements.back().figure = circle;
          collage->elements.back().pos = int2(0,0);
          collage->elements.back().size = collage->size;
        }
      }

      int pos_y = cur_y-cur_min_y+font.line_height*glyph_scale-collage->size.y;

      top_collage->elements.emplace_back();
      top_collage->elements.back().figure = collage;
      top_collage->elements.back().pos = int2(cur_x+cur_min_x,pos_y);
      top_collage->elements.back().size = collage->size;

      cur_x += glyph.advance.advanceWidth * glyph_scale + 1;
      if (cur_x > max_w)
      {
        cur_x = 0;
        cur_y += font.line_height * glyph_scale + 1;
      }
    }

    printf("min (%d,%d), max (%d,%d)\n", min_x, min_y, max_x, max_y);
    for (auto &elem : top_collage->elements)
    {
      elem.pos.x -= min_x;
      elem.pos.y -= min_y;
    }

    auto image = render_figure_to_image(top_collage);
    LiteImage::SaveImage("saves/glyphs.png", image);
  }

  void debug_render_text_bezier(const Font &font, std::vector<uint32_t> glyph_ids)
  {

  }

  bool read_ttf_debug(const std::string &filename)
  {
    Font font = read_ttf(filename);
    if (font.glyphs.size() == 0)
    {
      return false;
    }

    // due to different rendering conventions in ttf, we invert y axis here
    // for (auto &glyph : font.glyphs)
    // {
    //   for (int c = 0; c < glyph.contours.size(); c++)
    //   {
    //     for (int p = 0; p < glyph.contours[c].points.size(); p++)
    //     {
    //       int16_t y_rel = glyph.contours[c].points[p].y - glyph.yMin;
    //       glyph.contours[c].points[p].y = glyph.yMax - y_rel;
    //     }
    //   }
    // }
    
    std::vector<uint32_t> glyphs_to_render;
    // for (int c = 0; c < 32; c++)
    //   glyphs_to_render.push_back(c);
    // printf("line gap: %d\n", hheaTable.lineGap);
    // printf("line height: %d\n", font.line_height);
    // printf("< > = %d\n", font.cmap.unicodeToGlyph[' ']);
    // printf("space contains %d contours, %d advance\n", 
    //   (int)font.glyphs[font.cmap.unicodeToGlyph[' ']].contours.size(),
    //   font.glyphs[font.cmap.unicodeToGlyph[' ']].advance.advanceWidth);
    // printf("units per em: %d\n", headTable.unitsPerEm);
    const char *test_str = "The quick brown fox jumps over the lazy dog 0123456789";
    for (int i = 0; test_str[i] != 0; i++)
    {
      uint8_t ch = uint8_t(test_str[i]);
      glyphs_to_render.push_back(font.cmap.charGlyphs[ch]);
    }
    debug_render_text_bezier(font, glyphs_to_render);

    return true;
  }

  Font read_ttf(const std::string &filename)
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
    uint32_t cmap_table_id = (uint32_t)-1;
    uint32_t hhea_table_id = (uint32_t)-1;
    uint32_t hmtx_table_id = (uint32_t)-1;
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
      else if (strncmp("cmap", table_directories[i].tag, 4) == 0)
        cmap_table_id = i;
      else if (strncmp("hhea", table_directories[i].tag, 4) == 0)
        hhea_table_id = i;
      else if (strncmp("hmtx", table_directories[i].tag, 4) == 0)
        hmtx_table_id = i;
    }

    if (head_table_id == (uint32_t)-1)
    {
      printf("Error: no head table found\n");
      return Font();
    }
    if (maxp_table_id == (uint32_t)-1)
    {
      printf("Error: no maxp table found\n");
      return Font();
    }
    if (loca_table_id == (uint32_t)-1)
    {
      printf("Error: no loca table found\n");
      return Font();
    }
    if (glyph_table_id == (uint32_t)-1)
    {
      printf("Error: no glyf table found\n");
      return Font();
    }
    if (cmap_table_id == (uint32_t)-1)
    {
      printf("Error: no cmap table found\n");
      return Font();
    }
    if (hhea_table_id == (uint32_t)-1)
    {
      printf("Error: no hhea table found\n");
      return Font();
    }
    if (hmtx_table_id == (uint32_t)-1)
    {
      printf("Error: no hmtx table found\n");
      return Font();
    }

    TTFHeadTable headTable = read_head_table(buffer.data() + table_directories[head_table_id].offset);
    TTFMaxpTable maxpTable = read_maxp_table(buffer.data() + table_directories[maxp_table_id].offset);
    TTFCmapTable cmapTable = read_cmap_table(buffer.data() + table_directories[cmap_table_id].offset);
    TTFHorizontalHeaderTable hheaTable = read_hhea_table(buffer.data() + table_directories[hhea_table_id].offset);
    std::vector<TTFLongHorMetric> hmtxTable = read_hmtx_table(buffer.data() + table_directories[hmtx_table_id].offset,
                                                              hheaTable.numOfLongHorMetrics, maxpTable.maxGlyphs); 
    std::vector<uint32_t> all_glyph_locations = get_all_glyph_locations(
      buffer.data(), maxpTable.maxGlyphs, 
      headTable.indexToLocFormat == 0 ? 2 : 4,
      table_directories[loca_table_id].offset);

    TTFGlyphSet glyphSet;
    glyphSet.is_compound.resize(maxpTable.maxGlyphs);
    glyphSet.glyph_locations.resize(maxpTable.maxGlyphs);
    for (uint32_t glyphId = 0; glyphId < maxpTable.maxGlyphs; glyphId++)
    {
      uint32_t off = table_directories[glyph_table_id].offset + all_glyph_locations[glyphId];
      int number_of_contours = big_to_little_endian<int16_t>(buffer.data() + off);
      if (number_of_contours < 0)
      {
        glyphSet.is_compound[glyphId] = true;
        glyphSet.glyph_locations[glyphId] = glyphSet.compound_glyphs.size();
        //printf("read glyph %d/%d at offset %d (compound)\n", glyphId, maxpTable.maxGlyphs, off);
        glyphSet.compound_glyphs.push_back(read_compound_glyph(buffer.data() + off));
      }
      else
      {
        glyphSet.is_compound[glyphId] = false;
        glyphSet.glyph_locations[glyphId] = glyphSet.simple_glyphs.size();
        //printf("read glyph %d/%d at offset %d (simple)\n", glyphId, maxpTable.maxGlyphs, off);
        glyphSet.simple_glyphs.push_back(read_simple_glyph(buffer.data() + off));
      }
    }

    Font font;
    font.cmap = cmapTable;
    font.scale = 1.0f/float(headTable.unitsPerEm);
    for (uint32_t glyphId = 0; glyphId < maxpTable.maxGlyphs; glyphId++)
    {
      if (glyphSet.is_compound[glyphId])
      {
        font.glyphs.push_back(simplify_compound_glyph(
          glyphSet.compound_glyphs[glyphSet.glyph_locations[glyphId]], glyphSet));
      }
      else
      {
        font.glyphs.push_back(glyphSet.simple_glyphs[glyphSet.glyph_locations[glyphId]]);
      }
    }

    for (int i = 0; i < font.glyphs.size(); i++)
    {
      font.glyphs[i].advance = hmtxTable[i];
    }

    for (auto &glyph : font.glyphs)
    {
      add_implied_points(glyph);
    }

    fix_space_glyphs(font);
    font.line_height = hheaTable.ascent - hheaTable.descent + hheaTable.lineGap;
    font.ascent = hheaTable.ascent;
    font.descent = hheaTable.descent;

    return font;
  }
}