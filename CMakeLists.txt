cmake_minimum_required(VERSION 3.16)
project(litefigure C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build configuration to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_definitions(-DUSE_STB_IMAGE)

# Compiler flags for Debug/Release
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Add source directories
add_subdirectory(src/core)
add_subdirectory(src/cli)
add_subdirectory(src/tests)