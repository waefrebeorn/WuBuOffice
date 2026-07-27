# wubuexp -- exporters built on top of wubulayout (EXP cluster).
add_library(wubuexp STATIC exp.c)
target_include_directories(wubuexp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubuexp PUBLIC wubulayout)
