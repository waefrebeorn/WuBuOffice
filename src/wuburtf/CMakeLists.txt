# wuburtf: RTF writer.
add_library(wuburtf STATIC rtf.c)
target_include_directories(wuburtf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wuburtf PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_rtf test_rtf.c)
target_link_libraries(test_rtf PRIVATE wuburtf)
target_compile_options(test_rtf PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME rtf COMMAND test_rtf)
set_tests_properties(rtf PROPERTIES LABELS "rtf;libs")
