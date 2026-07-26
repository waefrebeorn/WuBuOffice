# wubucfb: MS-CFB / OLE2 compound-file container reader + writer (legacy substrate)
add_library(wubucfb STATIC cfb.c cfb_write.c)
target_include_directories(wubucfb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
