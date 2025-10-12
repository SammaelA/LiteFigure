file(GLOB CORE_SOURCES "*.cpp" "*.h")

set(CORE_SOURCES ${CORE_SOURCES} 
    ${CMAKE_SOURCE_DIR}/src/1st-party/blk/blk.cpp
    ${CMAKE_SOURCE_DIR}/src/1st-party/csv/csv.cpp
    ${CMAKE_SOURCE_DIR}/src/1st-party/LiteMath/Image2d.cpp
    ${CMAKE_SOURCE_DIR}/src/3rd-party/pdfgen.c)

add_library(core STATIC ${CORE_SOURCES})
target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR}/src/core 
                                       ${CMAKE_SOURCE_DIR}/src/3rd-party
                                       ${CMAKE_SOURCE_DIR}/src/1st-party)