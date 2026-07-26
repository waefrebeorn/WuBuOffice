# wubuoxml: OPC package writer + reader (built on wubuzip)
# parser.c lives in src/wubuxml (shared XML parser used by both writer + reader).
add_library(wubuoxml STATIC
  package.c rels_path.c reader.c extract.c docx_document.c
  ${CMAKE_SOURCE_DIR}/src/wubuxml/parser.c)
target_include_directories(wubuoxml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubuoxml PUBLIC wubuzip)
