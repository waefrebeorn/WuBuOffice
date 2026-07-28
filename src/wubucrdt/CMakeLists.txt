# wubucrdt: op-based node-sequence CRDT (convergence for local-first sync).
add_library(wubucrdt STATIC crdt.c)
target_include_directories(wubucrdt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubucrdt PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_crdt test_crdt.c)
target_link_libraries(test_crdt PRIVATE wubucrdt)
target_compile_options(test_crdt PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME crdt COMMAND test_crdt)
set_tests_properties(crdt PROPERTIES LABELS "crdt;libs")
