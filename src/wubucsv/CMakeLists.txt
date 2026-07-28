# wubucsv: CSV parser.
add_library(wubucsv STATIC csv.c)
target_include_directories(wubucsv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubucsv PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_wubucsv test_csv.c)
target_link_libraries(test_wubucsv PRIVATE wubucsv)
target_compile_options(test_wubucsv PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME wubucsv COMMAND test_wubucsv)
set_tests_properties(wubucsv PROPERTIES LABELS "wubucsv;libs")
