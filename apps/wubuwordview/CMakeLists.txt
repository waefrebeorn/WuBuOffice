# wubuwordview: offscreen document -> PNG renderer (proves the GUI plumbing).
# Uses system FreeType2 for glyph rasterization and zlib for PNG deflation.
# Renders a wubumodel_doc page with wubuspell red squiggles + a wubuchart
# bar chart embedded. No window/display required -> viewable artifact.
find_package(Freetype REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(wubuwordview main.c)
target_include_directories(wubuwordview PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel
    ${CMAKE_SOURCE_DIR}/src/wubuspell
    ${CMAKE_SOURCE_DIR}/src/wubuchart
    ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(wubuwordview PRIVATE
    wubumodel wubuspell wubuchart wububase ${FREETYPE_LIBRARIES} ZLIB::ZLIB m)
