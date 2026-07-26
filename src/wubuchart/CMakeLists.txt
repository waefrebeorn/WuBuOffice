# wubuchart: dependency-free C11 chart renderer (SVG: bar/line/pie/scatter).
# Builds SVG strings and finalizes them through wubusvg (parse/regurgitate).
# Optional formula subtitle via wubuformula.
add_library(wubuchart STATIC chart.c)
target_include_directories(wubuchart PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubusvg
    ${CMAKE_SOURCE_DIR}/src/wubuformula)
target_link_libraries(wubuchart PUBLIC wubusvg wubuformula m)
