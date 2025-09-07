file(GLOB CLI_SOURCES "*.cpp" "*.h")
add_executable(litefigure-cli ${CLI_SOURCES})
target_link_libraries(litefigure-cli core)
target_include_directories(litefigure-cli PRIVATE ${CMAKE_SOURCE_DIR}/src/cli
                                                  ${CMAKE_SOURCE_DIR}/src/core 
                                                  ${CMAKE_SOURCE_DIR}/src/3rd-party
                                                  ${CMAKE_SOURCE_DIR}/src/1st-party)