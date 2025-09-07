file(GLOB TEST_SOURCES "*.cpp" "*.h")
add_executable(test ${TEST_SOURCES})
target_link_libraries(test core)
target_include_directories(test PRIVATE ${CMAKE_SOURCE_DIR}/src/tests
                                        ${CMAKE_SOURCE_DIR}/src/core 
                                        ${CMAKE_SOURCE_DIR}/src/3rd-party
                                        ${CMAKE_SOURCE_DIR}/src/1st-party)