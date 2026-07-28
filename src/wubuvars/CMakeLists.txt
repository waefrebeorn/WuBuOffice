# wubuvars: document variables.
add_library(wubuvars STATIC vars.c)
target_include_directories(wubuvars PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubuvars PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_vars test_vars.c)
target_link_libraries(test_vars PRIVATE wubuvars)
target_compile_options(test_vars PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME vars COMMAND test_vars)
set_tests_properties(vars PROPERTIES LABELS "vars;libs")
