# wubuwatermark: page watermark config.
add_library(wubuwatermark STATIC watermark.c)
target_include_directories(wubuwatermark PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubuwatermark PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_watermark test_watermark.c)
target_link_libraries(test_watermark PRIVATE wubuwatermark)
target_compile_options(test_watermark PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME watermark COMMAND test_watermark)
set_tests_properties(watermark PROPERTIES LABELS "watermark;libs")
