# wubufmtpaint: format painter (needs wubumodel).
add_library(wubufmtpaint STATIC fmtpaint.c)
target_include_directories(wubufmtpaint PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel)
target_link_libraries(wubufmtpaint PUBLIC wubumodel)
target_compile_options(wubufmtpaint PRIVATE -Wall -Wextra -Wpedantic)
add_executable(test_fmtpaint test_fmtpaint.c)
target_link_libraries(test_fmtpaint PRIVATE wubufmtpaint wubumodel)
target_compile_options(test_fmtpaint PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME fmtpaint COMMAND test_fmtpaint)
set_tests_properties(fmtpaint PROPERTIES LABELS "fmtpaint;libs")
