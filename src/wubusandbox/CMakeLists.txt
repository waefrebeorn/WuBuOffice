# wubusandbox: plugin capability sandbox (deny-by-default).
add_library(wubusandbox STATIC sandbox.c)
target_include_directories(wubusandbox PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubusandbox PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_sandbox test_sandbox.c)
target_link_libraries(test_sandbox PRIVATE wubusandbox)
target_compile_options(test_sandbox PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME sandbox COMMAND test_sandbox)
set_tests_properties(sandbox PROPERTIES LABELS "sandbox;libs")
