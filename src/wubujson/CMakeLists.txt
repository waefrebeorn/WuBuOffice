# wubujson: minimal JSON parse+emit (vendored from WuBuPad, same API)
add_library(wubujson STATIC json.c)
target_include_directories(wubujson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
