# wubupdfextract: PDF text import (pdf_extract.c lives in wubuimage).
add_library(wubupdfextract STATIC pdfextract.c)
target_include_directories(wubupdfextract PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubupdf)
target_link_libraries(wubupdfextract PUBLIC wubuimage)
target_compile_options(wubupdfextract PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_pdfextract test_pdfextract.c)
target_link_libraries(test_pdfextract PRIVATE wubupdfextract)
target_compile_options(test_pdfextract PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME pdfextract COMMAND test_pdfextract)
set_tests_properties(pdfextract PROPERTIES LABELS "pdfextract;libs")
