# wubuepub: dependency-free C11 EPUB3 writer (wubuzip + wubumodel).
# Packages a wubumodel_doc into a valid EPUB 3 container.
add_library(wubuepub STATIC epub.c)
target_include_directories(wubuepub PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/wubumodel
    ${CMAKE_SOURCE_DIR}/src/wubuzip)
target_link_libraries(wubuepub PUBLIC wubumodel wubuzip wububase)
