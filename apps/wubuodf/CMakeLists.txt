# wubuodf: OpenDocument (odt/ods/odp) read + write over the three models
add_library(wubuodf_app OBJECT odf_pkg.c odf_body.c odf_text.c odf_sheet.c odf_pres.c odf_flat.c)
target_include_directories(wubuodf_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_SOURCE_DIR}/src/wubuxml
    ${CMAKE_SOURCE_DIR}/src/wubuzip
    ${CMAKE_SOURCE_DIR}/apps/wubuword
    ${CMAKE_SOURCE_DIR}/apps/wubuedit
    ${CMAKE_SOURCE_DIR}/apps/wubucell
    ${CMAKE_SOURCE_DIR}/apps/wubushow
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubuodf_app PRIVATE wubuoxml wubuxml wubuzip)
