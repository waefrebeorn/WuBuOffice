# wubuaislot: offline AI assist hook (provider slot + rule-based fallback).
add_library(wubuaislot STATIC aislot.c)
target_include_directories(wubuaislot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubuaislot PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_aislot test_aislot.c)
target_link_libraries(test_aislot PRIVATE wubuaislot)
target_compile_options(test_aislot PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME aislot COMMAND test_aislot)
set_tests_properties(aislot PROPERTIES LABELS "aislot;libs")
