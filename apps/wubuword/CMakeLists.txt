# wubuword: WordprocessingML document model + writer
add_library(wubuword_app OBJECT main.c word_doc.c word_table.c assemble.c)
target_include_directories(wubuword_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_SOURCE_DIR}/src/wubuxml
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubuword_app PRIVATE wubuoxml wubuxml)

add_executable(wubuword $<TARGET_OBJECTS:wubuword_app> entry.c)
target_link_libraries(wubuword PRIVATE wubuoxml wubuxml)
