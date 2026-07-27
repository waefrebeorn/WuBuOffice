# wubuwordview: offscreen document -> PNG renderer (proves the GUI plumbing).
# Rendering lives in src/wuburender; this app builds a doc, renders, writes PNG.
# Uses system FreeType2 (via wuburender) and zlib (via wubupng).
find_package(Freetype REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(wubuwordview main.c)
target_include_directories(wubuwordview PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel
    ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(wubuwordview PRIVATE
    wubumodel wuburender wubupng ${FREETYPE_LIBRARIES} m)
