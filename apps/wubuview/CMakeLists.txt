# wubuview: tabbed terminal document viewer (drag/drop + multi-format)
add_executable(wubuview entry.c main.c controller.c docflat.c)
target_include_directories(wubuview PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubujson
    ${CMAKE_SOURCE_DIR}/src/wubutui
    ${CMAKE_SOURCE_DIR}/src/wubudoc)
target_link_libraries(wubuview PRIVATE wubutui wubudoc wubujson)
