# wubua11yannounce: screen-reader announcement queue.
add_library(wubua11yannounce STATIC a11yannounce.c)
target_include_directories(wubua11yannounce PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubua11yannounce PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_a11yannounce test_a11yannounce.c)
target_link_libraries(test_a11yannounce PRIVATE wubua11yannounce)
target_compile_options(test_a11yannounce PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME a11yannounce COMMAND test_a11yannounce)
set_tests_properties(a11yannounce PROPERTIES LABELS "a11yannounce;libs")
