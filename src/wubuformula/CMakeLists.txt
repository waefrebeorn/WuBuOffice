# wubuformula: from-scratch formula engine (lexer/parser/AST/evaluator/functions)
add_library(wubuformula STATIC
  value.c value_util.c lexer.c ast.c parser.c eval.c
  functions/registry.c functions/logic.c functions/math.c
  functions/text.c functions/lookup.c functions/datefin.c functions/stat.c)
target_include_directories(wubuformula PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/functions)
target_link_libraries(wubuformula PUBLIC m)
