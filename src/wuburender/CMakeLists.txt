# wuburender: shared wubumodel_doc -> RGBA page renderer (FreeType + wubumodel +
# wubuspell + wubuchart). Used by both the offscreen PNG writer and the live
# SDL window so they never diverge. Uses system FreeType2.
find_package(Freetype REQUIRED)
add_library(wuburender STATIC wuburender.c wuburender.h)
target_include_directories(wuburender PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel
    ${CMAKE_SOURCE_DIR}/src/wubuspell
    ${CMAKE_SOURCE_DIR}/src/wubuchart
    ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(wuburender PUBLIC
    wubumodel wubuspell wubuchart wububase ${FREETYPE_LIBRARIES})
target_compile_options(wuburender PRIVATE -Wall -Wextra -Wpedantic)
