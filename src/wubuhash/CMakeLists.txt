# wubuhash: SHA-256.
add_library(wubuhash STATIC hash.c)
target_include_directories(wubuhash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubuhash PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_hash test_hash.c)
target_link_libraries(test_hash PRIVATE wubuhash)
target_compile_options(test_hash PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME hash COMMAND test_hash)
set_tests_properties(hash PROPERTIES LABELS "hash;libs")
