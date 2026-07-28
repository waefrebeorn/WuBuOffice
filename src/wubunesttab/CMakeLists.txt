# wubunesttab: nested tables builder/validator (needs wubumodel).
add_library(wubunesttab STATIC nesttab.c)
target_include_directories(wubunesttab PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel)
target_link_libraries(wubunesttab PUBLIC wubumodel)
target_compile_options(wubunesttab PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_nesttab test_nesttab.c)
target_link_libraries(test_nesttab PRIVATE wubunesttab wubumodel)
target_compile_options(test_nesttab PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME nesttab COMMAND test_nesttab)
set_tests_properties(nesttab PROPERTIES LABELS "nesttab;libs")
