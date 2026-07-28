# wubupasteplain: paste-plain stripper.
add_library(wubupasteplain STATIC pasteplain.c)
target_include_directories(wubupasteplain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(wubupasteplain PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_pasteplain test_pasteplain.c)
target_link_libraries(test_pasteplain PRIVATE wubupasteplain)
target_compile_options(test_pasteplain PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME pasteplain COMMAND test_pasteplain)
set_tests_properties(pasteplain PROPERTIES LABELS "pasteplain;libs")
