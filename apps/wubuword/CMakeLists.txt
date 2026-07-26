# wubuword: WordprocessingML document model + writer
# NOTE: word_spell.c is NOT part of the shared object lib (it pulls in
# wubuspell); it is compiled directly into the standalone wubuword binary so
# other consumers of wubuword_app (wubuconv/wuburead/wubuedit) don't inherit
# the spell dependency.
add_library(wubuword_app OBJECT main.c word_doc.c word_table.c assemble.c)
target_include_directories(wubuword_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_SOURCE_DIR}/src/wubuxml
    ${CMAKE_SOURCE_DIR}/src/wubuspell
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubuword_app PRIVATE wubuoxml wubuxml)

add_executable(wubuword $<TARGET_OBJECTS:wubuword_app> entry.c word_spell.c)
target_include_directories(wubuword PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuspell)
target_link_libraries(wubuword PRIVATE wubuoxml wubuxml wubuspell)
