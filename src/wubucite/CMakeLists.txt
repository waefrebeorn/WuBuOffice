# wubucite: bibliography / citation store.
add_library(wubucite STATIC cite.c)
target_include_directories(wubucite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubucite PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_cite test_cite.c)
target_link_libraries(test_cite PRIVATE wubucite)
target_compile_options(test_cite PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME cite COMMAND test_cite)
set_tests_properties(cite PROPERTIES LABELS "cite;libs")
