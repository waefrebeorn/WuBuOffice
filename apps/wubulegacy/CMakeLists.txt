# wubulegacy: readers + writers for legacy binary Office (.doc/.xls/.ppt) over MS-CFB
add_library(wubulegacy_app OBJECT
    xls_biff.c doc_bin.c ppt_bin.c
    legacy_xls_write.c legacy_doc_write.c legacy_ppt_write.c)
target_include_directories(wubulegacy_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubucfb
    ${CMAKE_SOURCE_DIR}/apps/wubucell
    ${CMAKE_SOURCE_DIR}/apps/wubushow
    ${CMAKE_SOURCE_DIR}/apps/wubuedit
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubulegacy_app PRIVATE wubucfb)
