# wubulang: per-node language attribute.
add_library(wubulang STATIC lang.c)
target_include_directories(wubulang PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubulang PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_lang test_lang.c)
target_link_libraries(test_lang PRIVATE wubulang)
target_compile_options(test_lang PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME lang COMMAND test_lang)
set_tests_properties(lang PROPERTIES LABELS "lang;libs")
