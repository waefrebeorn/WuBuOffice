# wubuwordwin: interactive WuBuWord window (SDL2 over wuburender).
# The same render path apps/wubuwordview uses, presented live with scroll +
# a typeable input line. Uses system SDL2. Does NOT touch apps/wubuword
# (the separate WordprocessingML editor).
find_package(SDL2 REQUIRED)

add_executable(wubuwordwin main.c)
target_include_directories(wubuwordwin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel
    ${SDL2_INCLUDE_DIRS})
target_link_libraries(wubuwordwin PRIVATE
    wubumodel wuburender ${SDL2_LIBRARIES})
target_compile_options(wubuwordwin PRIVATE -Wall -Wextra -Wpedantic)
