# wubumodel: WS11 unified object model (shared substrate for all apps)
add_library(wubumodel STATIC
  model.c model_io.c doc_cache.c load_async.c)
target_include_directories(wubumodel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wubumodel PRIVATE wubuoxml pthread)
