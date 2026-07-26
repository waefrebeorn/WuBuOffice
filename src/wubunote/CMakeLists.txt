# wubunote_core: pure text-buffer model (Notepad++-class editing core).
# Self-contained, allocation-checked, unit-tested without a TTY.
add_library(wubunote_core STATIC edit.c)
target_include_directories(wubunote_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
