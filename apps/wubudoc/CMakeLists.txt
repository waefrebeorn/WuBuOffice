# wubudoc_app: Markdown + HTML + RTF + JSON serializers over the models
add_library(wubudoc_app OBJECT doc_md.c doc_rtf.c model_json.c epub.c)
target_include_directories(wubudoc_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_SOURCE_DIR}/src/wubuxml
    ${CMAKE_SOURCE_DIR}/src/wubuzip
    ${CMAKE_SOURCE_DIR}/apps/wubuword
    ${CMAKE_SOURCE_DIR}/apps/wubuedit
    ${CMAKE_SOURCE_DIR}/apps/wubucell
    ${CMAKE_SOURCE_DIR}/apps/wubushow
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubudoc_app PRIVATE wubuoxml wubuxml wubuzip)
