# wububase: shared internal utilities (utf8, dynamic string buffer, xml escape).
# Dependency-free. No third-party C. 0 warnings under -Wall -Wextra -Wpedantic.
add_library(wububase STATIC
    wububase.c
    wububase.h)
target_include_directories(wububase PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wububase PRIVATE -Wall -Wextra -Wpedantic)
