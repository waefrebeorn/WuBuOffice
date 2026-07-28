# wubufocus: visible focus indicator config.
add_library(wubufocus STATIC focus.c)
target_include_directories(wubufocus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubufocus PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_focus test_focus.c)
target_link_libraries(test_focus PRIVATE wubufocus)
target_compile_options(test_focus PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME focus COMMAND test_focus)
set_tests_properties(focus PROPERTIES LABELS "focus;libs")
