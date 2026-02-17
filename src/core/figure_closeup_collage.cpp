#include "figure.h"

#include <cstdio>
#include <unordered_map>

namespace LiteFigure
{
  bool position_is_supported(ElementPosition pos)
  {
    return (pos == ElementPosition::Right)  ||
           (pos == ElementPosition::Left)   ||
           (pos == ElementPosition::Bottom) ||
           (pos == ElementPosition::Top);
  }

  bool create_crop_frame(int2 size, int2 base_image_size, float default_frame_thickness, float4 default_frame_color, const Block *blk, 
                         std::shared_ptr<LiteFigure::Rectangle> &out_frame)
  {
    // set default parameters for frame
    Block o_frame_blk;
    o_frame_blk.set_ivec2("size", size);
    o_frame_blk.set_double("thickness", default_frame_thickness);
    o_frame_blk.set_vec4("color", default_frame_color);

    // add additional parameters from block and override defaults
    o_frame_blk.add_detalization(*blk->get_block("frame"));

    // convert to pixel thickness relative to main image
    int th_pixel = o_frame_blk.get_double("thickness") * std::max(base_image_size.x, base_image_size.y);
    o_frame_blk.set_int("thickness_pixel", th_pixel);

    bool main_frame_loaded = out_frame->load(&o_frame_blk);
    if (!main_frame_loaded)
    {
      fprintf(stderr, "[CloseUpCollage::load] ERROR: failed to load main frame\n");
      return false;
    }
    return true;
  }

  bool CloseUpCollage::load(const Block *blk)
  {
    constexpr int MAX_CROPS_COUNT = 8;

    const float4 default_frame_color = float4(0, 0, 0, 1);
    const float  default_frame_thickness = 0.005f;
    const float4 default_font_color = float4(0, 0, 0, 1);
    const float  default_font_size_mult  = 1.0f/16.0f;

    auto main_image_collage = std::make_shared<Collage>();

    Block *image_blk = blk->get_block("image");
    if (!image_blk)
    {
      fprintf(stderr, "[CloseUpCollage::load] ERROR: no 'image' block\n");
      return false;
    }

    //preloading image, it will be reloaded later as part of full collage,
    //but the actual loading from disk happens only once thank to ImageCache
    auto image_fig = std::make_shared<PrimitiveImage>();
    bool image_loaded = image_fig->load(image_blk);
    if (!image_loaded)
    {
      fprintf(stderr, "[CloseUpCollage::load] ERROR: failed to load image\n");
      return false;
    }

    float4 base_crop = blk->get_vec4("base_crop", float4(0,0,1,1));
    int2 full_image_size = image_fig->calculateSize();
    int2 base_image_size = int2(full_image_size.x*(base_crop.z-base_crop.x),
                                full_image_size.y*(base_crop.w-base_crop.y));
    if (base_image_size.x < 1 || base_image_size.y < 1)
    {
      fprintf(stderr, "[CloseUpCollage::load] ERROR: invalid crop parameters\n");
      return false;
    }

    //add transform block with image
    {
      Block o_transform_blk;
      o_transform_blk.set_vec4("crop", base_crop);
      o_transform_blk.set_block("figure", image_blk);
      o_transform_blk.get_block("figure")->set_enum("type", "FigureType", (uint32_t)FigureType::PrimitiveImage);

      auto main_image_crop = std::make_shared<Transform>();
      bool main_image_loaded = main_image_crop->load(&o_transform_blk);
      if (!main_image_loaded)
      {
        fprintf(stderr, "[CloseUpCollage::load] ERROR: failed to load main image cropped figure\n");
        return false;
      }
      main_image_collage->elements.push_back(Collage::Element(int2(0,0), base_image_size, main_image_crop));
    }

    //add frame if it is set
    if (blk->get_block("frame"))
    {
      auto main_frame = std::make_shared<Rectangle>();
      bool loaded_frame = create_crop_frame(base_image_size, base_image_size, default_frame_thickness, default_frame_color, blk, main_frame);
      if (!loaded_frame)
        return false;
      main_image_collage->elements.push_back(Collage::Element(int2(0,0), base_image_size, main_frame));
    }

    //add text if it is set
    if (blk->get_block("text"))
    {
      int default_font_size = std::min(base_image_size.x, base_image_size.y) * default_font_size_mult;
      int2 pos = int2(default_font_size / 8 + 1);
      int2 size = int2(base_image_size.x, -1);

      Block o_text_blk;
      o_text_blk.set_int("font_size", default_font_size);
      o_text_blk.set_vec4("color", default_font_color);

      //add additional parameters from block and override defaults
      o_text_blk.add_detalization(*blk->get_block("text"));

      auto main_text = std::make_shared<Text>();
      bool main_text_loaded = main_text->load(&o_text_blk);
      if (!main_text_loaded)
      {
        fprintf(stderr, "[CloseUpCollage::load] ERROR: failed to load main text\n");
        return false;
      }
      main_image_collage->elements.push_back(Collage::Element(pos, size, main_text));
    }

    //add crops to map
    std::unordered_map<ElementPosition, std::vector<const Block *>> crops_by_position;
    for (int crop_n=0; crop_n<MAX_CROPS_COUNT; crop_n++)
    {
      Block *crop_blk = blk->get_block("crop_"+std::to_string(crop_n));
      if (!crop_blk)
        continue;
      
      ElementPosition ep = (ElementPosition)crop_blk->get_enum("position", (uint32_t)ElementPosition::Manual);
      if (!position_is_supported(ep))
      {
        fprintf(stderr, "[CloseUpCollage::load] ERROR:Crop position %d is not supported\n", (int)ep);
        return false;
      }

      crops_by_position[ep].push_back(crop_blk);
    }

    std::unordered_map<ElementPosition,std::shared_ptr<Grid>> crop_grids;
    for (auto &[position, crops] : crops_by_position)
    {
      bool x_step = position == ElementPosition::Right || position == ElementPosition::Left;

      auto crop_grid = std::make_shared<Grid>();
      crop_grid->rows.resize(x_step ? crops.size() : 1);
      uint32_t row_n = 0;
      for (auto &crop_blk : crops)
      {
        float4 crop_rel = crop_blk->get_vec4("crop", float4(0,0,1,1));
        float2 bc_p = float2(base_crop.x, base_crop.y);
        float2 bc_s = float2(base_crop.z-base_crop.x, base_crop.w-base_crop.y);
        float4 crop = float4(bc_p.x + bc_s.x*crop_rel.x, bc_p.y + bc_s.y*crop_rel.y,
                             bc_p.x + bc_s.x*crop_rel.z, bc_p.y + bc_s.y*crop_rel.w);
        
        auto sc_collage = std::make_shared<Collage>();

        Block o_transform_blk;
        o_transform_blk.set_vec4("crop", crop);
        o_transform_blk.set_block("figure", image_blk);
        o_transform_blk.get_block("figure")->set_enum("type", "FigureType", (uint32_t)FigureType::PrimitiveImage);

        auto sc_transform = std::make_shared<Transform>();
        sc_transform->load(&o_transform_blk);
        int2 sc_transform_size = sc_transform->calculateSize();
        sc_collage->elements.push_back(Collage::Element(int2(0,0), sc_transform_size, sc_transform));

        if (crop_blk->get_block("frame"))
        {
          //frame around cropped fragment
          auto sc_frame = std::make_shared<Rectangle>();
          create_crop_frame(sc_transform_size, base_image_size, default_frame_thickness, default_frame_color, crop_blk, sc_frame);
          sc_collage->elements.push_back(Collage::Element(int2(0,0), sc_transform_size, sc_frame));

          //frame in place where it was cropped
          int2 from_frame_pos  = int2(crop_rel.x*base_image_size.x, crop_rel.y*base_image_size.y);
          int2 from_frame_size = int2((crop_rel.z-crop_rel.x)*base_image_size.x, (crop_rel.w-crop_rel.y)*base_image_size.y);
          auto from_frame = std::make_shared<Rectangle>();
          create_crop_frame(from_frame_size, base_image_size, default_frame_thickness, default_frame_color, crop_blk, from_frame);
          main_image_collage->elements.push_back(Collage::Element(from_frame_pos, from_frame_size, from_frame));
        }

        if (x_step)
        {
          crop_grid->rows[row_n].push_back(sc_collage);
          row_n++;
        }
        else
        {
          crop_grid->rows[0].push_back(sc_collage);
        }
      }

      int2 max_size = int2(1,1);
      for (auto &row : crop_grid->rows)
      {
        for (auto &elem : row)
        {
          int2 size = elem->calculateSize();
          if (x_step)
            max_size.x = std::max(max_size.x, size.x);
          else
            max_size.y = std::max(max_size.y, size.y);
        }
      }
      for (auto &row : crop_grid->rows)
      {
        for (auto &elem : row)
        {
          int2 cur_size = elem->size;
          float2 scale = max(float2(max_size) / float2(cur_size), float2(1,1));
          float q = std::max(scale.x, scale.y);
          elem->size = int2(float2(cur_size)*q);
        }
      }

      int2 full_size = crop_grid->calculateSize();
      float ratio = x_step ? float(base_image_size.y)/full_size.y : float(base_image_size.x)/full_size.x;
      int2 target_size = int2(ratio*full_size.x, ratio*full_size.y);
      crop_grid->calculateSize(target_size);
      crop_grids[position] = crop_grid;
    }

    int2 main_image_size = main_image_collage->calculateSize();
    auto main_grid = std::make_shared<Grid>();

    if (crop_grids.find(ElementPosition::Top) != crop_grids.end())
    {
      //TODO: if left crop is present, insert fill
      main_grid->rows.emplace_back();
      main_grid->rows.back().push_back(crop_grids[ElementPosition::Top]);
    }

    main_grid->rows.emplace_back();
    if (crop_grids.find(ElementPosition::Left) != crop_grids.end())
      main_grid->rows.back().push_back(crop_grids[ElementPosition::Left]);
    main_grid->rows.back().push_back(main_image_collage);
    if (crop_grids.find(ElementPosition::Right) != crop_grids.end())
      main_grid->rows.back().push_back(crop_grids[ElementPosition::Right]);
    
    if (crop_grids.find(ElementPosition::Bottom) != crop_grids.end())
    {
      //TODO: if left crop is present, insert fill
      main_grid->rows.emplace_back();
      main_grid->rows.back().push_back(crop_grids[ElementPosition::Bottom]);
    }

    main_grid->size = blk->get_ivec2("size");
    int2 r_size = main_grid->calculateSize();
    m_figure = main_grid;
    return true;
  }
}