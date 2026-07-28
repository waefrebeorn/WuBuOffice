# wubuhistory: opaque version-history store.
add_library(wubuhistory STATIC history.c)
target_include_directories(wubuhistory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubuhistory PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_history test_history.c)
target_link_libraries(test_history PRIVATE wubuhistory)
target_compile_options(test_history PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME history COMMAND test_history)
set_tests_properties(history PROPERTIES LABELS "history;libs")
