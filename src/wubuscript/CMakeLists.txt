# wubuscript: dependency-free C11 safe scripting host (over wubuformula).
# Sandboxed expression evaluator with a caller-supplied variable resolver and
# a math/aggregate-only function registry (no FS/process calls).
add_library(wubuscript STATIC script.c)
target_include_directories(wubuscript PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubuformula)
target_link_libraries(wubuscript PUBLIC wubuformula)
