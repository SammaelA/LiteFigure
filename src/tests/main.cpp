#include <cstdio>
#include "regression.h"


int main(int argc, char *argv[]) 
{
  if (argc == 1 || std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")
  {
    printf("Run all tests:             ./test run\n");
    printf("Run specific tests:        ./test run [test_number_1] [test_number_2] ... [test_number_n]\n");
    printf("Recreate reference images: ./test rebuild [test_number_1] [test_number_2] ... [test_number_n]\n");
  }
  else
  {
    bool recreate_reference_images = false;
    if (std::string(argv[1]) == "rebuild")
    {
      recreate_reference_images = true;
    }
    else if (std::string(argv[1]) == "run")
    {
      recreate_reference_images = false;
    }
    else
    {
      printf("Unknown command %s\n", argv[1]);
      return 1;
    }

    std::vector<int> test_numbers;
    for (int i = 2; i < argc; i++)
    {
      test_numbers.push_back(std::stoi(argv[i]));
    }
    LiteFigure::perform_regression_tests(test_numbers, recreate_reference_images);
  }
  return 0;
}