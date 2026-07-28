# wubua11ytree: UI -> a11y tree serializer (needs wubumodel).
add_library(wubua11ytree STATIC a11ytree.c)
target_include_directories(wubua11ytree PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel)
target_compile_options(wubua11ytree PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_a11ytree test_a11ytree.c)
target_include_directories(test_a11ytree PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/wubumodel)
target_link_libraries(test_a11ytree PRIVATE wubua11ytree wubumodel)
target_compile_options(test_a11ytree PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME a11ytree COMMAND test_a11ytree)
set_tests_properties(a11ytree PROPERTIES LABELS "a11ytree;libs")
