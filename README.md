# LiteFigure
Making figures from images and much more

    cmake -S . -B build_debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build_debug -j16

    cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release && cmake --build build_release -j16

Code coverage

    cmake -S . -B build_profile -DCMAKE_BUILD_TYPE=PROFILE && cmake --build build_profile -j32 && ./build_profile/src/tests/test run && cd build_profile && gcovr -r .. . --txt ../saves/coverage_report.txt; cd ..