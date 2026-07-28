# wubuscope: table header-row scope markup.
add_library(wubuscope STATIC scope.c)
target_include_directories(wubuscope PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubuscope PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_scope test_scope.c)
target_link_libraries(test_scope PRIVATE wubuscope)
target_compile_options(test_scope PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME scope COMMAND test_scope)
set_tests_properties(scope PROPERTIES LABELS "scope;libs")
