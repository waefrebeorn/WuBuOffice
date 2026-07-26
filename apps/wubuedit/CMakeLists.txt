# wubuedit: editing core + document model glue
add_library(wubuedit_app OBJECT edit.c docmodel.c)
target_include_directories(wubuedit_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_SOURCE_DIR}/src/wubuxml
    ${CMAKE_SOURCE_DIR}/apps/wubuword
    ${CMAKE_SOURCE_DIR}/apps/wubucell
    ${CMAKE_SOURCE_DIR}/apps/wubushow
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubuedit_app PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)

add_executable(wubuedit $<TARGET_OBJECTS:wubuedit_app> entry.c)
target_link_libraries(wubuedit PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)
