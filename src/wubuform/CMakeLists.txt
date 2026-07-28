# wubuform: PDF form-field data model.
add_library(wubuform STATIC form.c)
target_include_directories(wubuform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubuform PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_form test_form.c)
target_link_libraries(test_form PRIVATE wubuform)
target_compile_options(test_form PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME form COMMAND test_form)
set_tests_properties(form PROPERTIES LABELS "form;libs")
