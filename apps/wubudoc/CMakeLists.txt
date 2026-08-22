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

# wubudoc_serve: NDJSON agent server (stdin/stdout). The AGI/OS bus entry:
#   echo '{"cmd":"open","path":"x.docx"}' | wubudoc_serve
add_executable(wubudoc_serve agent_main.c)
target_link_libraries(wubudoc_serve PRIVATE wubudoc wubujson wubuoxml wubuzip wubuxml wubusvg wubuconv wubucfb wubuformula wubumodel m)
target_include_directories(wubudoc_serve PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubudoc ${CMAKE_SOURCE_DIR}/src/wubujson
    ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuzip
    ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/src/wubusvg
    ${CMAKE_SOURCE_DIR}/src/wubucfb ${CMAKE_SOURCE_DIR}/src/wubuformula
    ${CMAKE_SOURCE_DIR}/src/wubumodel)
