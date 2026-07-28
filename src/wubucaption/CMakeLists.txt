# wubucaption: figure/table captions.
add_library(wubucaption STATIC caption.c)
target_include_directories(wubucaption PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubucaption PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_caption test_caption.c)
target_link_libraries(test_caption PRIVATE wubucaption)
target_compile_options(test_caption PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME caption COMMAND test_caption)
set_tests_properties(caption PROPERTIES LABELS "caption;libs")
