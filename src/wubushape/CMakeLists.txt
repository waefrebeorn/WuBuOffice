# wubushape -- minimal Bidi visual reordering (INT-7: RTL rendering).
add_library(wubushape STATIC shape.c)
target_include_directories(wubushape PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
