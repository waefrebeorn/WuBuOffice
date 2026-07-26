# wubumath: dependency-free C11 math expression renderer (SVG).
# Pragmatic LaTeX-ish subset (\frac, ^sup, _sub, parentheses, atoms);
# box-model layout, finalized through wubusvg.
add_library(wubumath STATIC math.c)
target_include_directories(wubumath PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubusvg)
target_link_libraries(wubumath PUBLIC wubusvg m)
