# wubuxps: minimal XPS writer (store-only ZIP + FixedPage).
add_library(wubuxps STATIC xps.c)
target_include_directories(wubuxps PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubuxps PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_xps test_xps.c)
target_link_libraries(test_xps PRIVATE wubuxps)
target_compile_options(test_xps PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME xps COMMAND test_xps)
set_tests_properties(xps PROPERTIES LABELS "xps;libs")
