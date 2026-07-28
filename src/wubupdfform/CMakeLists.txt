# wubupdfform: real AcroForm PDF writer (needs wubuform).
add_library(wubupdfform STATIC pdfform.c)
target_include_directories(wubupdfform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubuform)
target_link_libraries(wubupdfform PUBLIC wubuform)
target_compile_options(wubupdfform PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_pdfform test_pdfform.c)
target_link_libraries(test_pdfform PRIVATE wubupdfform wubuform)
target_compile_options(test_pdfform PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME pdfform COMMAND test_pdfform)
set_tests_properties(pdfform PROPERTIES LABELS "pdfform;libs")
