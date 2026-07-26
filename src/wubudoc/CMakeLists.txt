# wubudoc: unified document ingestion + creation facade (wubuOS bus)
add_library(wubudoc STATIC
    wubudoc.c
    b64.c
    agent.c)
target_include_directories(wubudoc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubujson
    ${CMAKE_SOURCE_DIR}/src/wubusvg
    ${CMAKE_SOURCE_DIR}/src/wubufont
    ${CMAKE_SOURCE_DIR}/src/wubuzip
    ${CMAKE_SOURCE_DIR}/src/wubucfb
    ${CMAKE_SOURCE_DIR}/src/wubuxml)
# wuburead_app + wubuconv are defined in apps/ (added after src/); CMake
# resolves the forward link reference at generate time.
target_link_libraries(wubudoc PUBLIC
    wubusvg wubufont wubuzip wubucfb wubuxml wubujson wubuoxml
    wuburead_app wubuconv wubuimage)
