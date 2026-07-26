# wubuoffice: unified CLI (embeds image2doc front-end)
add_executable(wubuoffice main.c ${CMAKE_SOURCE_DIR}/tools/image2doc.c)
target_compile_definitions(wubuoffice PRIVATE WUBEOFFICE_EMBED)
target_include_directories(wubuoffice PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/apps/wubuconv
    ${CMAKE_SOURCE_DIR}/src/wubuocr
    ${CMAKE_SOURCE_DIR}/src/wubuimage
    ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(wubuoffice PRIVATE
    wubuoxml wubuxml wubuzip wubuformula wubucfb wubujson wubuconv crnnocr wubuimage wubufont m)
