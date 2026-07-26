# wubushow: presentation model
add_library(wubushow_app OBJECT main.c show_model.c show_io.c show_read.c)
target_include_directories(wubushow_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubushow_app PRIVATE wubuoxml)

add_executable(wubushow $<TARGET_OBJECTS:wubushow_app> entry.c)
target_link_libraries(wubushow PRIVATE wubuoxml)
