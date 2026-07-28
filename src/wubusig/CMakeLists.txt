# wubusig: HMAC-SHA256 document signature (needs wubuhash).
add_library(wubusig STATIC sig.c)
target_include_directories(wubusig PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubuhash)
target_link_libraries(wubusig PUBLIC wubuhash)
target_compile_options(wubusig PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_sig test_sig.c)
target_link_libraries(test_sig PRIVATE wubusig wubuhash)
target_compile_options(test_sig PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME sig COMMAND test_sig)
set_tests_properties(sig PROPERTIES LABELS "sig;libs")
