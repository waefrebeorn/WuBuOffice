cmake_minimum_required(VERSION 3.10)
project(WuBuOffice C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Expose POSIX functions (strdup, open_memstream) under -std=c11 so pointer-
# returning POSIX calls are not implicitly truncated to int.
add_compile_definitions(_POSIX_C_SOURCE=200809L)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# --- wubuzip: from-scratch ZIP + DEFLATE (reader and writer) ---
add_library(wubuzip STATIC
  src/wubuzip/crc.c
  src/wubuzip/zip.c
  src/wubuzip/reader.c
  src/wubuzip/bit.c
  src/wubuzip/bitw.c
  src/wubuzip/canon.c
  src/wubuzip/huffman.c
  src/wubuzip/fixed.c
  src/wubuzip/fixedcode.c
  src/wubuzip/limitcode.c
  src/wubuzip/lz77.c
  src/wubuzip/deflate.c
  src/wubuzip/block.c
  src/wubuzip/inflate.c)
target_include_directories(wubuzip PUBLIC src/wubuzip)

# --- wubuxml: streaming XML writer ---
add_library(wubuxml STATIC src/wubuxml/xml.c)
target_include_directories(wubuxml PUBLIC src/wubuxml)

# --- wubuformula: from-scratch formula engine (lexer/parser/AST/evaluator/functions) ---
add_library(wubuformula STATIC
  src/wubuformula/value.c
  src/wubuformula/value_util.c
  src/wubuformula/lexer.c
  src/wubuformula/ast.c
  src/wubuformula/parser.c
  src/wubuformula/eval.c
  src/wubuformula/functions/registry.c
  src/wubuformula/functions/logic.c
  src/wubuformula/functions/math.c
  src/wubuformula/functions/text.c
  src/wubuformula/functions/lookup.c
  src/wubuformula/functions/datefin.c
  src/wubuformula/functions/stat.c)
target_include_directories(wubuformula PUBLIC
  src/wubuformula
  src/wubuformula/functions)
target_link_libraries(wubuformula PUBLIC m)

# --- wubumodel: WS11 unified object model (shared substrate for all apps) ---
add_library(wubumodel STATIC
  src/wubumodel/model.c
  src/wubumodel/model_io.c)
target_include_directories(wubumodel PUBLIC src/wubumodel)
target_link_libraries(wubumodel PRIVATE wubuoxml)

# --- wubuoxml: OPC package writer + reader (built on wubuzip) ---
add_library(wubuoxml STATIC
  src/wubuoxml/package.c
  src/wubuoxml/rels_path.c
  src/wubuoxml/reader.c
  src/wubuoxml/docx_text.c)
target_include_directories(wubuoxml PUBLIC src/wubuoxml)
target_link_libraries(wubuoxml PUBLIC wubuzip)

# --- app object libraries (shared between standalone exe and unified CLI) ---
add_library(wubuword_app OBJECT
  apps/wubuword/main.c apps/wubuword/word_doc.c apps/wubuword/word_table.c apps/wubuword/assemble.c)
target_include_directories(wubuword_app PRIVATE src/wubuoxml src/wubuxml apps/wubuword)
target_link_libraries(wubuword_app PRIVATE wubuoxml wubuxml)

add_library(wubucell_app OBJECT apps/wubucell/main.c apps/wubucell/cell_model.c apps/wubucell/cell_eval.c apps/wubucell/cell_io.c apps/wubucell/style.c)
target_include_directories(wubucell_app PRIVATE src/wubuoxml apps/wubucell src/wubuformula)
target_link_libraries(wubucell_app PRIVATE wubuoxml wubuformula)

add_library(wubushow_app OBJECT apps/wubushow/main.c apps/wubushow/show_model.c apps/wubushow/show_io.c)
target_include_directories(wubushow_app PRIVATE src/wubuoxml apps/wubushow)
target_link_libraries(wubushow_app PRIVATE wubuoxml)

add_library(wuburead_app OBJECT apps/wuburead/main.c)
target_include_directories(wuburead_app PRIVATE src/wubuoxml src/wubuzip src/wubuxml)
target_link_libraries(wuburead_app PRIVATE wubuoxml)

add_library(wubuedit_app OBJECT apps/wubuedit/edit.c)
target_include_directories(wubuedit_app PRIVATE src/wubuoxml apps/wubuword)
target_link_libraries(wubuedit_app PRIVATE wubuoxml wubuword_app)

# --- standalone executables ---
add_executable(wubuword $<TARGET_OBJECTS:wubuword_app> apps/wubuword/entry.c)
target_link_libraries(wubuword PRIVATE wubuoxml wubuxml)
add_executable(wubucell $<TARGET_OBJECTS:wubucell_app> apps/wubucell/entry.c)
target_link_libraries(wubucell PRIVATE wubuoxml wubuformula)
add_executable(wubushow $<TARGET_OBJECTS:wubushow_app> apps/wubushow/entry.c)
target_link_libraries(wubushow PRIVATE wubuoxml)
add_executable(wuburead $<TARGET_OBJECTS:wuburead_app> apps/wuburead/entry.c)
target_link_libraries(wuburead PRIVATE wubuoxml)
add_executable(wubuedit $<TARGET_OBJECTS:wubuedit_app> apps/wubuedit/entry.c)
target_link_libraries(wubuedit PRIVATE wubuoxml wubuword_app)

# --- unified CLI ---
add_executable(wubuoffice apps/wubuoffice/main.c
  $<TARGET_OBJECTS:wubuword_app>
  $<TARGET_OBJECTS:wubucell_app>
  $<TARGET_OBJECTS:wubushow_app>
  $<TARGET_OBJECTS:wuburead_app>
  $<TARGET_OBJECTS:wubuedit_app>)
target_link_libraries(wubuoffice PRIVATE wubuoxml wubuxml wubuformula)

# --- tests ---
enable_testing()
add_executable(test_wubuzip tests/test_wubuzip.c)
target_link_libraries(test_wubuzip PRIVATE wubuzip)
add_test(NAME wubuzip_crc COMMAND test_wubuzip)

add_executable(test_inflate tests/test_inflate.c)
target_link_libraries(test_inflate PRIVATE wubuzip z)
add_test(NAME inflate COMMAND test_inflate)

add_executable(test_formula tests/test_formula.c)
target_link_libraries(test_formula PRIVATE wubuformula)
target_include_directories(test_formula PRIVATE src/wubuformula/functions)
add_test(NAME formula COMMAND test_formula)

add_executable(test_model tests/test_model.c)
target_link_libraries(test_model PRIVATE wubumodel)
add_test(NAME model COMMAND test_model)

add_executable(test_model_io tests/test_model_io.c)
target_link_libraries(test_model_io PRIVATE wubumodel wubuoxml)
target_include_directories(test_model_io PRIVATE src/wubumodel src/wubuoxml)
add_test(NAME model_io COMMAND test_model_io)

add_executable(test_reader tests/test_reader.c)
target_link_libraries(test_reader PRIVATE wubuoxml wubuword_app)
target_include_directories(test_reader PRIVATE src/wubuoxml src/wubuxml apps/wubuword)
add_test(NAME reader_roundtrip COMMAND test_reader)

add_executable(test_edit tests/test_edit.c)
target_link_libraries(test_edit PRIVATE wubuoxml wubuword_app wubuedit_app)
target_include_directories(test_edit PRIVATE src/wubuoxml src/wubuxml apps/wubuword apps/wubuedit)
add_test(NAME edit_roundtrip COMMAND test_edit)
