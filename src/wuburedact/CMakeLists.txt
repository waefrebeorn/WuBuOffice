# wuburedact: redaction set + export.
add_library(wuburedact STATIC redact.c)
target_include_directories(wuburedact PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wuburedact PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_redact test_redact.c)
target_link_libraries(test_redact PRIVATE wuburedact)
target_compile_options(test_redact PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME redact COMMAND test_redact)
set_tests_properties(redact PROPERTIES LABELS "redact;libs")
