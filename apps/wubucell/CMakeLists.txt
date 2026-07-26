# wubucell: spreadsheet model (cells, formulas, merge, charts, read/write)
add_library(wubucell_app OBJECT main.c cell_model.c cell_eval.c cell_io.c style.c cell_read.c cell_csv.c)
target_include_directories(wubucell_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_SOURCE_DIR}/src/wubuxml
    ${CMAKE_SOURCE_DIR}/src/wubuformula
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubucell_app PRIVATE wubuoxml wubuxml wubuformula)

add_executable(wubucell $<TARGET_OBJECTS:wubucell_app> entry.c)
target_link_libraries(wubucell PRIVATE wubuoxml wubuformula)
