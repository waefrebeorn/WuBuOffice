# wubusvg: clean-room SVG ingest + regurgitate (document engine)
# parser.c is the shared src/wubuxml XML parser.
add_library(wubusvg STATIC
  wubusvg.c agent.c rast.c
  ${CMAKE_SOURCE_DIR}/src/wubuxml/parser.c)
target_include_directories(wubusvg PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src/wubuxml)
target_link_libraries(wubusvg PUBLIC wubuxml wubujson)
