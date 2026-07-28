# wubuoffice: unified launcher that boots the wubuos shell.
# Installed next to wubuos so it can exec the sibling binary.
add_executable(wubuoffice wubuoffice.c)
set_target_properties(wubuoffice PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps/wubuos)
target_compile_options(wubuoffice PRIVATE -Wall -Wextra -Wpedantic)
