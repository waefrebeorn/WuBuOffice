# wubupdf_app: minimal PDF/1.7 writer over the TEXT model (dm_doc).
# Note: pdf_extract.c is compiled into the wubuimage lib (src/wubuimage).
add_library(wubupdf_app OBJECT pdf_text.c pdf_write.c)
target_include_directories(wubupdf_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/apps/wubuedit)
