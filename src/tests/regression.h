#pragma once
#include <vector>
#include <string>

namespace LiteFigure
{
  // performs regression tests, if test_numbers is empty, performs all tests
  // if recreate_reference_images is true, recreates reference images, tests marked as passed
  // returns number of failed tests
  int perform_regression_tests(std::vector<int> test_numbers, bool recreate_reference_images = false);
}