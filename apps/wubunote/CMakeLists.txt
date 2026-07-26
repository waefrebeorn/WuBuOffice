# wubunote: tabbed terminal text editor (Notepad++-class UI)
add_executable(wubunote entry.c main.c note_controller.c)
target_include_directories(wubunote PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubunote
    ${CMAKE_SOURCE_DIR}/src/wubutui
    ${CMAKE_SOURCE_DIR}/src/wubudoc)
target_link_libraries(wubunote PRIVATE wubunote_core wubutui wubudoc)
