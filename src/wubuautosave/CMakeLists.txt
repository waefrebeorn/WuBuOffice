# wubuautosave: dependency-free C11 autosave + crash-recovery.
# Atomic ".asd" snapshots of a wubumodel_doc + PID ".lock" crash detection.
# Uses wubumodel for serialization; POSIX file ops + CLOCK_MONOTONIC.
add_library(wubuautosave STATIC autosave.c)
target_include_directories(wubuautosave PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel)
target_link_libraries(wubuautosave PUBLIC wubumodel)
