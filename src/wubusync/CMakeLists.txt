# wubusync: local-first CRDT store + shared lock (depends on wubucrdt).
add_library(wubusync STATIC sync.c)
target_include_directories(wubusync PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubucrdt)
target_link_libraries(wubusync PUBLIC wubucrdt)
target_compile_options(wubusync PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_sync test_sync.c)
target_link_libraries(test_sync PRIVATE wubusync wubucrdt)
target_compile_options(test_sync PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME sync COMMAND test_sync)
set_tests_properties(sync PROPERTIES LABELS "sync;libs")
