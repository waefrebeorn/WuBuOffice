# wuburead: reverse-direction readers (rtf/html/epub -> dm_doc)
add_library(wuburead_app OBJECT main.c read_rtf.c read_html.c read_epub.c)
target_include_directories(wuburead_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_SOURCE_DIR}/src/wubuzip
    ${CMAKE_SOURCE_DIR}/src/wubuxml
    ${CMAKE_SOURCE_DIR}/apps/wubuedit
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wuburead_app PRIVATE wubuoxml wubuxml wubuzip wubuedit_app)

add_executable(wuburead $<TARGET_OBJECTS:wuburead_app> entry.c)
target_link_libraries(wuburead PRIVATE wubuoxml wubuxml wubuzip wubuedit_app wubuword_app wubucell_app wubushow_app)
