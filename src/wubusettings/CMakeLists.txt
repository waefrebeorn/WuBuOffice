# wubusettings -- persistent config (opaque, JSON-backed).
add_library(wubusettings STATIC settings.c)
target_include_directories(wubusettings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubusettings PUBLIC wubujson)
