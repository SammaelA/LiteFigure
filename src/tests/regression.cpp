#include "regression.h"
#include "figure.h"

#include "LiteMath/LiteMath.h"
#include "LiteMath/Image2d.h"

#include <filesystem>
#include <algorithm>

using LiteMath::float4;

namespace LiteFigure
{
  float PSNR(const LiteImage::Image2D<float4> &image_1, const LiteImage::Image2D<float4> &image_2)
  {
    assert(image_1.vector().size() == image_2.vector().size());
    unsigned sz = image_1.vector().size();
    double sum = 0.0;
    for (int i = 0; i < sz; i++)
    {
      float r1 = image_1.vector()[i].x;
      float g1 = image_1.vector()[i].y;
      float b1 = image_1.vector()[i].z;
      float r2 = image_2.vector()[i].x;
      float g2 = image_2.vector()[i].y;
      float b2 = image_2.vector()[i].z;
      sum += ((r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2)) / (3.0f);
    }
    float mse = sum / sz;

    return -10 * log10(std::max<double>(1e-10, mse));
  }

  int perform_regression_tests(std::vector<int> test_numbers, bool recreate_reference_images)
  {
    constexpr int PSNR_THR = 50.0f; // no visual difference, perfect match with a few pixels difference
    const std::string blk_dir = "figures";
    const std::string images_dir = "reference_images";
    const std::string failed_tests_dir = "saves/failed_tests";

    std::vector<Block*> test_blks;
    std::vector<std::string> ref_image_paths;
    if (test_numbers.empty())
    {
      std::vector<std::filesystem::directory_entry> filenames;
      for (const auto &entry : std::filesystem::directory_iterator(blk_dir))
      {
        if (entry.path().extension() == ".blk")
          filenames.push_back(entry);
      }

      std::sort(filenames.begin(), filenames.end());

      for (const auto &entry : filenames)
      {
        Block blk;
        bool loaded = load_block_from_file(entry.path().string(), blk);
        if (!loaded)
          printf("[perform_regression_tests] failed to load test config %s\n", entry.path().string().c_str());
        else
        {
          std::string ref_image_path = images_dir + "/" + entry.path().stem().string() + ".png";
          if (!recreate_reference_images && !std::filesystem::exists(ref_image_path))
            printf("[perform_regression_tests] reference image %s does not exist\n", ref_image_path.c_str());
          else
          {
            test_blks.emplace_back(new Block());
            test_blks.back()->copy(&blk);
            ref_image_paths.push_back(ref_image_path);
            test_numbers.push_back(std::stoi(entry.path().stem().string().substr(1)));
          }
        }
      }
    }
    else
    {
      for (int i = 0; i < test_numbers.size(); i++)
      {
        Block blk;
        std::string path = blk_dir + "/f" + std::to_string(test_numbers[i]) + ".blk";
        bool loaded = load_block_from_file(path, blk);
        if (!loaded)
          printf("[perform_regression_tests] failed to load test %d (path = %s)\n", i, path.c_str());
        else
        {
          std::string ref_image_path = images_dir + "/f" + std::to_string(test_numbers[i]) + ".png";
          if (!recreate_reference_images && !std::filesystem::exists(ref_image_path))
            printf("[perform_regression_tests] reference image %s does not exist\n", ref_image_path.c_str());
          else
          {
            test_blks.emplace_back(new Block());
            test_blks.back()->copy(&blk);
            ref_image_paths.push_back(ref_image_path);
          }
        }
      }
    }

    if (!std::filesystem::exists(failed_tests_dir))
      std::filesystem::create_directories(failed_tests_dir);
    else
      std::filesystem::remove_all(failed_tests_dir);

    int failed_tests = 0;
    for (int test_i = 0; test_i < test_blks.size(); test_i++)
    {
      int test_num = test_numbers[test_i];
      std::string result_msg = "";

      const Block *blk = test_blks[test_i];

      FigurePtr fig = create_figure_from_blk(blk);
      LiteImage::Image2D<float4> out = render_figure_to_image(fig);

      delete blk;

      if (recreate_reference_images)
      {
        bool saved = LiteImage::SaveImage(ref_image_paths[test_i].c_str(), out);
        result_msg = saved ? "Reference image recreated" : "Failed to save reference image!";
        failed_tests += !saved;
      }
      else
      {
        LiteImage::Image2D<float4> ref_image = LiteImage::LoadImage<float4>(ref_image_paths[test_i].c_str());
        float psnr = PSNR(out, ref_image);

        if (psnr >= PSNR_THR)
        {
          result_msg = "PASSED";
        }
        else
        {
          // save failed test image
          std::string path = failed_tests_dir + "/" + std::to_string(test_num) + ".png";
          LiteImage::SaveImage(path.c_str(), out);
          result_msg = "FAILED (PSNR = " + std::to_string(psnr) + ")";
          failed_tests++;
        }
      }

      printf("[%02d] %s\n", test_num, result_msg.c_str());
    }
    
    printf("%d/%d tests failed\n", failed_tests, (int)test_blks.size());

    return failed_tests;
  }
}