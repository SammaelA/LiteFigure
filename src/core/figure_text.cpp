#include "figure.h"
#include "templates.h"
#include "renderer.h"
#include "font.h"
#include <cstdio>

namespace LiteFigure
{
	REGISTER_ENUM(TextAlignmentX,
				  ([]()
				   { return std::vector<std::pair<std::string, unsigned>>{
						 {"Left", (unsigned)TextAlignmentX::Left},
						 {"Right", (unsigned)TextAlignmentX::Right},
						 {"Center", (unsigned)TextAlignmentX::Center},
					 }; })());

	REGISTER_ENUM(TextAlignmentY,
				  ([]()
				   { return std::vector<std::pair<std::string, unsigned>>{
						 {"Top", (unsigned)TextAlignmentY::Top},
						 {"Bottom", (unsigned)TextAlignmentY::Bottom},
						 {"Center", (unsigned)TextAlignmentY::Center},
					 }; })());

	bool Text::load(const Block *blk)
	{
		text = blk->get_string("text", text);
		font_name = blk->get_string("font_name", font_name);
		color = blk->get_vec4("color", color);
		background_color = blk->get_vec4("background_color", background_color);
		size = blk->get_ivec2("size", size);
		retain_width = blk->get_bool("retain_width", retain_width);
		retain_height = blk->get_bool("retain_height", retain_height);
		font_size = blk->get_int("font_size", font_size);
		alignment_x = (TextAlignmentX)blk->get_enum("alignment_x", (uint32_t)alignment_x);
		alignment_y = (TextAlignmentY)blk->get_enum("alignment_y", (uint32_t)alignment_y);
		verbose = blk->get_bool("verbose", verbose);
		return true;
	}

	bool Glyph::load(const Block *blk)
	{
		glyph_id = blk->get_int("glyph");
		font_name = blk->get_string("font_name");
		color = blk->get_vec4("color");
		return true;
	}

	int2 Text::placeGlyphs()
	{
		glyphs.clear();
		glyph_positions.clear();

		const Font &font = get_font(font_name);
		float glyph_scale = font_size * font.scale;

		int cur_x = 0;
		int cur_y = 0;
		int max_x = 0;

		int last_space = -1;
		int word_start_g = 0;
		int line_start = 0;
		int line_start_g = 0;
		std::vector<int> line_ends;
		for (int c_id = 0; c_id < text.size(); c_id++)
		{
			uint32_t gId = font.cmap.charGlyphs[text[c_id]];
			const TTFSimpleGlyph &glyph = font.glyphs[gId];
			float2 sz = float2(glyph.xMax - glyph.xMin, glyph.yMax - glyph.yMin);
			int cur_min_x = int(glyph_scale * float(glyph.xMin));
			int cur_min_y = int(glyph_scale * float(glyph.yMin));

			// to make sure our text is not cut off on the left
			if (c_id == 0)
				cur_x = -cur_min_x;

			int2 g_size = int2(glyph_scale * sz) + int2(1, 1);
			int pos_y = cur_y - cur_min_y + font.line_height * glyph_scale - g_size.y;
			int2 g_pos = int2(cur_x + cur_min_x, pos_y);

			if (text[c_id] == ' ' || text[c_id] == '\t')
			{
				last_space = c_id;
				word_start_g = glyphs.size();
				cur_x += glyph.advance.advanceWidth * glyph_scale;
				if (size.x > 0 && cur_x > size.x)
				{
					line_start = c_id + 1;
					line_start_g = glyphs.size();
					line_ends.push_back(glyphs.size() - 1);
					cur_x = 0;
					cur_y += font.line_height * glyph_scale;
				}
				continue;
			}

			if (text[c_id] == '\n')
			{
				last_space = c_id;
				word_start_g = glyphs.size();
				line_start = c_id + 1;
				line_start_g = glyphs.size();
				line_ends.push_back(glyphs.size() - 1);
				cur_x = 0;
				cur_y += font.line_height * glyph_scale;
				continue;
			}

			cur_x += glyph.advance.advanceWidth * glyph_scale;
			if (verbose)
				printf("(%c): %d/%d\n", text[c_id], cur_x, size.x);

			if (size.x > 0 && cur_x > size.x)
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

					line_ends.push_back(word_start_g - 1);
				}
				else // the word in too large, we have to split it
				{
					cur_x = glyph.advance.advanceWidth * glyph_scale;
					cur_y += font.line_height * glyph_scale;
					g_pos = int2(0, pos_y + font.line_height * glyph_scale);
					line_ends.push_back(glyphs.size() - 1);
				}
			}
			max_x = std::max(max_x, cur_x);

			Glyph g;
			g.size = g_size;
			g.color = color;
			g.font_name = font_name;
			g.glyph_id = gId;
			g.character = text[c_id];
			g.font_size = font_size;
			glyphs.push_back(g);
			glyph_positions.push_back(g_pos);
			if (verbose)
			{
				printf("added glyph %d, pos %d %d, size %d %d\n", gId, g_pos.x, g_pos.y, g_size.x, g_size.y);
			}
		}

		line_ends.push_back(glyphs.size() - 1);

		// calculate proper size
		int2 proper_size = int2(max_x + 1, 0);
		for (int i = 0; i < glyphs.size(); i++)
		{
			proper_size.y = std::max(proper_size.y, glyph_positions[i].y + glyphs[i].size.y);
			if (verbose)
			{
				printf("glyph %d, pos %d %d, size %d %d\n", i, glyph_positions[i].x, glyph_positions[i].y, glyphs[i].size.x, glyphs[i].size.y);
			}
		}
		if (verbose)
		{
			printf("proper size %d %d\n", proper_size.x, proper_size.y);
		}

		if (retain_width)
			proper_size.x = std::max(proper_size.x, size.x);
		if (retain_height)
			proper_size.y = std::max(proper_size.y, size.y);

		// apply alignment
		if (retain_width && alignment_x != TextAlignmentX::Left)
		{
			int line_start = 0;
			for (int l_id = 0; l_id < line_ends.size(); l_id++)
			{
				int2 text_box_size = int2(0, 0);
				for (int i = line_start; i <= line_ends[l_id]; i++)
					text_box_size = max(text_box_size, glyph_positions[i] + glyphs[i].size);

				int gap = proper_size.x - text_box_size.x;
				int shift = 0;
				if (alignment_x == TextAlignmentX::Right)
					shift = gap;
				else if (alignment_x == TextAlignmentX::Center)
					shift = gap / 2;

				for (int i = line_start; i <= line_ends[l_id]; i++)
					glyph_positions[i].x += shift;

				line_start = line_ends[l_id] + 1;
			}
		}

		if (retain_height && alignment_y != TextAlignmentY::Top)
		{
			// int line_start = 0;
			// for (int l_id=0;l_id<line_ends.size();l_id++)
			{
				int2 text_box_size = int2(0, 0);
				for (int i = 0; i < glyphs.size(); i++)
					text_box_size = max(text_box_size, glyph_positions[i] + glyphs[i].size);

				int gap = proper_size.y - text_box_size.y;
				int shift = 0;
				if (alignment_y == TextAlignmentY::Bottom)
					shift = gap;
				else if (alignment_y == TextAlignmentY::Center)
					shift = gap / 2;

				for (int i = 0; i < glyphs.size(); i++)
					glyph_positions[i].y += shift;
			}
		}

		return proper_size;
	}

	int2 Text::calculateSize(int2 force_size)
	{
		int2 prev_size = size;
		if (is_valid_size(force_size))
		{
			float scale = std::min(float(force_size.x) / float(size.x), float(force_size.y) / float(size.y));
			font_size *= scale;
			size = force_size;
		}
		if (!equal(prev_size, size) || glyphs.empty())
		size = placeGlyphs();
		if (verbose)
		{
			printf("[Text %s] size %d %d font size %d\n", text.c_str(), size.x, size.y, font_size);
		}
		return size;
	}

	void Text::prepareInstances(int2 pos, std::vector<Instance> &out_instances)
	{
		if (background_color.w > 0)
		{
			background_fill.size = size;
			background_fill.color = background_color;
			Instance inst;
			inst.prim = &background_fill;
			inst.data.pos = pos;
			inst.data.size = size;
			out_instances.push_back(inst);
		}
		for (int i = 0; i < glyphs.size(); i++)
		{
			Glyph &glyph = glyphs[i];
			int2 glyph_pos = glyph_positions[i];
			glyph_pos += pos;
			Instance inst;
			inst.prim = &glyph;
			inst.data.pos = glyph_pos;
			inst.data.size = glyph.size;
			out_instances.push_back(inst);
		}
	}
}