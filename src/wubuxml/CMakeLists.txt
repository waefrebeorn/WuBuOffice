# wubuxml: streaming XML writer
add_library(wubuxml STATIC xml.c)
target_include_directories(wubuxml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
