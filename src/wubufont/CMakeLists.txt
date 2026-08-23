# wubufont: clean-room SFNT/TrueType parser + SVG font emitter
add_library(wubufont STATIC wubufont.c woff.c odttf.c)
target_include_directories(wubufont PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src/wubuzip)
target_link_libraries(wubufont PUBLIC wubuzip m)
