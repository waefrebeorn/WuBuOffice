# wubucol: opaque comment-thread store.
add_library(wubucol STATIC col.c)
target_include_directories(wubucol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubucol PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_col test_col.c)
target_link_libraries(test_col PRIVATE wubucol)
target_compile_options(test_col PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME col COMMAND test_col)
set_tests_properties(col PROPERTIES LABELS "col;libs")
