#include "figure.h"
#include "templates.h"
#include "renderer.h"
#include "font.h"
#include <cstdio>

namespace LiteFigure
{
	bool Text::load(const Block *blk)
	{
		text = blk->get_string("text");
		font_name = blk->get_string("font_name");
		color = blk->get_vec4("color");
		size = blk->get_ivec2("size", size);
		retain_width = blk->get_bool("retain_width", retain_width);
		retain_height = blk->get_bool("retain_height", retain_height);
		font_size = blk->get_int("font_size", font_size);
		return true;
	}

	bool Glyph::load(const Block *blk)
	{
		glyph_id = blk->get_int("glyph");
		font_name = blk->get_string("font_name");
		color = blk->get_vec4("color");
		return true;
	}

  static bool is_valid_size(int2 size) { return size.x > 0 && size.y > 0; }
  static bool equal(int2 a, int2 b) { return a.x == b.x && a.y == b.y; }

	int2 Text::calculateSize(int2 force_size)
	{
		// external force_size has highest priority, but if
		// figure size is explicitly set, it is forced to children
		if (!is_valid_size(force_size) && is_valid_size(size))
			force_size = size;

		const Font &font = get_font(font_name);
		float glyph_scale = font_size * font.scale;

		int cur_x = 0;
		int cur_y = 0;

		int min_x = 0;
		int min_y = 0;
		int max_x = 0;
		int max_y = 0;

		int last_space = -1;
		int word_start_g = 0;
		int line_start = 0;
		int line_start_g = 0;
		for (int c_id = 0; c_id < text.size(); c_id++)
		{
			uint32_t gId = font.cmap.charGlyphs[text[c_id]];
			const TTFSimpleGlyph &glyph = font.glyphs[gId];
			float2 sz = float2(glyph.xMax - glyph.xMin, glyph.yMax - glyph.yMin);
			int cur_min_x = int(glyph_scale * float(glyph.xMin));
			int cur_min_y = int(glyph_scale * float(glyph.yMin));

			min_x = std::min(min_x, cur_x + int(glyph_scale * float(glyph.xMin)));
			min_y = std::min(min_y, cur_y + int(glyph_scale * float(glyph.yMin)));
			max_x = std::max(max_x, cur_x + int(glyph_scale * float(glyph.xMax)));
			max_y = std::max(max_y, cur_y + int(glyph_scale * float(glyph.yMax)));

			int2 g_size = int2(glyph_scale * sz) + int2(1, 1);
			int pos_y = cur_y - cur_min_y + font.line_height * glyph_scale - g_size.y;
			int2 g_pos = int2(cur_x + cur_min_x, pos_y);

			if (text[c_id] == ' ' || text[c_id] == '\t')
			{
				last_space = c_id;
				word_start_g = glyphs.size();
				cur_x += glyph.advance.advanceWidth * glyph_scale;
				if (cur_x > size.x)
				{
					line_start = c_id+1;
					line_start_g = glyphs.size();
					cur_x = 0;
					cur_y += font.line_height * glyph_scale;
				}	
				continue;
			}

			if (text[c_id] == '\n')
			{
				last_space = c_id;
				word_start_g = glyphs.size();
				line_start = c_id+1;
				line_start_g = glyphs.size();
				cur_x = 0;
				cur_y += font.line_height * glyph_scale;
				continue;	
			}

			cur_x += glyph.advance.advanceWidth * glyph_scale;
			if (cur_x > size.x)
			{
				// start from the beginning of the word, move it to
				// the next line
				if (last_space > line_start)
				{
					cur_x = 0;
					cur_y += font.line_height * glyph_scale;
					int y_off = font.line_height * glyph_scale;
					int x_off = -glyph_positions[word_start_g].x;
					for (int wc_id = word_start_g; wc_id < glyphs.size(); wc_id++)
					{
						glyph_positions[wc_id] += int2(x_off, y_off);
						cur_x += glyphs[wc_id].size.x;
					}
					g_pos = int2(cur_x + cur_min_x, pos_y + y_off);
					cur_x += glyph.advance.advanceWidth * glyph_scale;
				}
				else //the word in too large, we have to split it
				{
					cur_x = glyph.advance.advanceWidth * glyph_scale;
					cur_y += font.line_height * glyph_scale;
					g_pos = int2(0, pos_y + font.line_height * glyph_scale);
				}
			}

			Glyph g;
			g.size = g_size;
			g.color = color;
			g.font_name = font_name;
			g.glyph_id = gId;
			glyphs.push_back(g);
			glyph_positions.push_back(g_pos);
			// printf("added glyph %d, pos %d %d, size %d %d\n", gId, g_pos.x, g_pos.y, g_size.x, g_size.y);
		}

		//calculate proper size
		int2 proper_size = int2(0, 0);
		for (int i=0;i<glyphs.size();i++)
		{
			proper_size = max(proper_size, glyph_positions[i] + glyphs[i].size);
		}
		
		if (retain_width)
			proper_size.x = std::max(proper_size.x, size.x);
		if (retain_height)
			proper_size.y = std::max(proper_size.y, size.y);

		// rescale so that text fits into the given size
		if (is_valid_size(force_size))
		{
			float scale = std::min(float(force_size.x) / float(proper_size.x), float(force_size.y) / float(proper_size.y));
			int2 new_size = proper_size * scale;
			for (int i = 0; i < glyphs.size(); i++)
			{
				glyph_positions[i] *= scale;
				glyphs[i].size *= scale;
				new_size = max(new_size, glyph_positions[i] + glyphs[i].size);
			}
			size = new_size;
		}
		else
		{
			size = proper_size;
		}

		return size;
	}

	void Text::prepare_glyphs(int2 pos, std::vector<Instance> &out_instances)
	{
		for (int i=0;i<glyphs.size();i++)
		{
			Glyph &glyph = glyphs[i];
			int2 glyph_pos = glyph_positions[i];
			glyph_pos += pos;
			Instance inst;
			inst.prim = std::make_shared<Glyph>(glyph);
			inst.data.pos = glyph_pos;
			inst.data.size = glyph.size;
			out_instances.push_back(inst);
		}
	}
}