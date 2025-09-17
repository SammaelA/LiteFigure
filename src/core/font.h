#pragma once
#include <vector>
#include <map>
#include <cstdint>
#include <string>

namespace LiteFigure
{
	//not a cmap table structure from the file, but our format to use
  struct TTFCmapTable
  {
    uint16_t charGlyphs[256]; //map from char code to glyph index, only 256 chars supported
    std::map<uint32_t, uint16_t> unicodeToGlyph; //map from 4-bytes unicode codepoint to glyph index
  };

	struct TTFLongHorMetric
  {
    uint16_t advanceWidth;
    int16_t  leftSideBearing; 
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
    TTFLongHorMetric advance; //not in glyf table, but we store it here for convenience
    std::vector<Contour> contours;
  };  
	
  struct Font
  {
    float scale = 1/1024.0f;
    int16_t line_height = 0; //in FUnits
    int16_t ascent = 0; //in FUnits
    int16_t descent = 0; //in FUnits
    std::vector<TTFSimpleGlyph> glyphs;
    TTFCmapTable cmap;
  };

  // load font from file first time, then returns it from cache
  const Font &get_font(const std::string &filename);
}