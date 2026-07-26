# wubudraw: dependency-free C11 vector drawing model (SVG: rect/ellipse/line/poly/text).
# Builds SVG strings, finalized through wubusvg (parse/regurgitate).
add_library(wubudraw STATIC draw.c)
target_include_directories(wubudraw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubusvg)
target_link_libraries(wubudraw PUBLIC wubusvg)
