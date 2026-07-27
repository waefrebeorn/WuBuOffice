# wubutoc -- table-of-contents generator (DOC-54); thin consumer of
# wubumodel + wubulayout.
add_library(wubutoc STATIC toc.c)
target_include_directories(wubutoc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubutoc PUBLIC wubumodel wubulayout)
