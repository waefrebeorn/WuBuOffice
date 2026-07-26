# wubufont CLI: sfnt (ttf/otf) -> SVG <font> document
add_executable(wubufont_cli main.c)
target_link_libraries(wubufont_cli PRIVATE wubufont)
