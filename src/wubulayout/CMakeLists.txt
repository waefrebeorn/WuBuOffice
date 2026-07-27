# wubulayout -- central text pipeline (model -> laid-out pages).
add_library(wubulayout STATIC layout.c)
target_include_directories(wubulayout PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubulayout PUBLIC wubumodel wubushape)
