# wubuexp_png: selection -> PNG export (needs wubupng).
add_library(wubuexp_png STATIC exp_png.c)
target_include_directories(wubuexp_png PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubupng)
target_link_libraries(wubuexp_png PUBLIC wubupng)
target_compile_options(wubuexp_png PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_exp_png test_exp_png.c)
target_link_libraries(test_exp_png PRIVATE wubuexp_png)
target_compile_options(test_exp_png PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME exp_png COMMAND test_exp_png)
set_tests_properties(exp_png PROPERTIES LABELS "exp_png;libs")
