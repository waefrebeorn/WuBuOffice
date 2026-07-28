# wubueqnum: equation numbering (needs wubumodel).
add_library(wubueqnum STATIC eqnum.c)
target_include_directories(wubueqnum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel)
target_compile_options(wubueqnum PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_eqnum test_eqnum.c)
target_include_directories(test_eqnum PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/wubumodel)
target_link_libraries(test_eqnum PRIVATE wubueqnum wubumodel)
target_compile_options(test_eqnum PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME eqnum COMMAND test_eqnum)
set_tests_properties(eqnum PROPERTIES LABELS "eqnum;libs")
