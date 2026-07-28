# wubuheading: heading-level enforcement (needs wubumodel).
add_library(wubuheading STATIC heading.c)
target_include_directories(wubuheading PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel)
target_compile_options(wubuheading PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_heading test_heading.c)
target_include_directories(test_heading PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/wubumodel)
target_link_libraries(test_heading PRIVATE wubuheading wubumodel)
target_compile_options(test_heading PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME heading COMMAND test_heading)
set_tests_properties(heading PROPERTIES LABELS "heading;libs")
