# wubuconv: unified format conversion over the semantic models
add_library(wubuconv_app OBJECT conv_map.c conv_bridge.c model_from_json.c)
target_include_directories(wubuconv_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_SOURCE_DIR}/src/wubuxml
    ${CMAKE_SOURCE_DIR}/src/wubuzip
    ${CMAKE_SOURCE_DIR}/src/wubucfb
    ${CMAKE_SOURCE_DIR}/apps/wubuword
    ${CMAKE_SOURCE_DIR}/apps/wubuedit
    ${CMAKE_SOURCE_DIR}/apps/wubucell
    ${CMAKE_SOURCE_DIR}/apps/wubushow
    ${CMAKE_SOURCE_DIR}/apps/wubudoc
    ${CMAKE_SOURCE_DIR}/apps/wubuodf
    ${CMAKE_SOURCE_DIR}/apps/wubulegacy
    ${CMAKE_SOURCE_DIR}/apps/wubupdf
    ${CMAKE_SOURCE_DIR}/apps/wubuconv
    ${CMAKE_SOURCE_DIR}/apps/wuburead
    ${CMAKE_SOURCE_DIR}/src/wubujson
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubuconv_app PUBLIC wubuoxml wubuxml wubuzip wubucfb wuburead_app wubujson wubumodel)
target_include_directories(wubuconv_app PRIVATE ${CMAKE_SOURCE_DIR}/src/wubumodel)

# A real static lib wrapping the conv objects, so downstream link ordering
# (wubujson must precede the conv objects) is handled by CMake automatically.
# It also bundles wubushow_app/wubucell_app objects because conv_map calls into
# both (docx<->pptx bridges etc.), making wubuconv self-contained.
add_library(wubuconv STATIC
    $<TARGET_OBJECTS:wubuconv_app>
    $<TARGET_OBJECTS:wubushow_app>
    $<TARGET_OBJECTS:wubucell_app>
    $<TARGET_OBJECTS:wubuword_app>
    $<TARGET_OBJECTS:wubuedit_app>
    $<TARGET_OBJECTS:wubudoc_app>
    $<TARGET_OBJECTS:wubuodf_app>
    $<TARGET_OBJECTS:wubulegacy_app>
    $<TARGET_OBJECTS:wubupdf_app>)
target_link_libraries(wubuconv PUBLIC
    wubuoxml wubuxml wubuzip wubucfb wuburead_app wubujson wubuformula wubumodel)
