# wubudyslexia: dyslexia-friendly mode config.
add_library(wubudyslexia STATIC dyslexia.c)
target_include_directories(wubudyslexia PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubudyslexia PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_dyslexia test_dyslexia.c)
target_link_libraries(test_dyslexia PRIVATE wubudyslexia)
target_compile_options(test_dyslexia PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME dyslexia COMMAND test_dyslexia)
set_tests_properties(dyslexia PROPERTIES LABELS "dyslexia;libs")
