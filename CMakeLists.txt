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

# Optional AddressSanitizer + UndefinedBehaviorSanitizer build. Enables the
# leak/UB gate used by the CI "sanitizer" job. Off by default so the normal
# build stays fast and dependency-free.
option(WITH_SANITIZER "Build with -fsanitize=address,undefined (leak/UB gate)" OFF)
if(WITH_SANITIZER)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1)
  add_link_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
endif()

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
  src/wubumodel/model_io.c
  src/wubumodel/doc_cache.c
  src/wubumodel/load_async.c)
target_include_directories(wubumodel PUBLIC src/wubumodel)
target_link_libraries(wubumodel PRIVATE wubuoxml pthread)

# ws07#1339: this is a NATIVE office suite. Reject Electron / JVM / any
# bundled web runtime at configure time so a dependency can never silently
# pull one in. Native C11 + pthreads only.
if(DEFINED ENV{ELECTRON})
  message(FATAL_ERROR "WuBuOffice is native-only: ELECTRON is not allowed "
          "(ws07#1339). Build with plain CMake, no Electron toolchain.")
endif()
if(DEFINED ENV{WUBOFFICE_ALLOW_JVM})
  message(WARNING "WUBOFFICE_ALLOW_JVM set: linking a JVM is permitted only for "
          "explicit server-side tooling, never the document apps (ws07#1339).")
endif()
find_library(JVM_LIB jvm)
if(JVM_LIB AND NOT WUBOFFICE_ALLOW_JVM)
  message(FATAL_ERROR "libjvm found on the link path and WUBOFFICE_ALLOW_JVM is "
          "not set. WuBuOffice document apps must not link the JVM (ws07#1339). "
          "Set -DWUBOFFICE_ALLOW_JVM=ON only for server-side tooling.")
endif()

# --- wubuoxml: OPC package writer + reader (built on wubuzip) ---
add_library(wubuoxml STATIC
  src/wubuoxml/package.c
  src/wubuoxml/rels_path.c
  src/wubuoxml/reader.c
  src/wubuoxml/extract.c
  src/wubuoxml/docx_document.c
  src/wubuxml/parser.c)
target_include_directories(wubuoxml PUBLIC src/wubuoxml)
target_link_libraries(wubuoxml PUBLIC wubuzip)

# --- app object libraries (shared between standalone exe and unified CLI) ---
add_library(wubuword_app OBJECT
  apps/wubuword/main.c apps/wubuword/word_doc.c apps/wubuword/word_table.c apps/wubuword/assemble.c)
target_include_directories(wubuword_app PRIVATE src/wubuoxml src/wubuxml apps/wubuword)
target_link_libraries(wubuword_app PRIVATE wubuoxml wubuxml)

add_library(wubucell_app OBJECT apps/wubucell/main.c apps/wubucell/cell_model.c apps/wubucell/cell_eval.c apps/wubucell/cell_io.c apps/wubucell/style.c apps/wubucell/cell_read.c)
target_include_directories(wubucell_app PRIVATE src/wubuoxml src/wubuxml apps/wubucell src/wubuformula)
target_link_libraries(wubucell_app PRIVATE wubuoxml wubuxml wubuformula)

add_library(wubushow_app OBJECT apps/wubushow/main.c apps/wubushow/show_model.c apps/wubushow/show_io.c apps/wubushow/show_read.c)
target_include_directories(wubushow_app PRIVATE src/wubuoxml apps/wubushow)
target_link_libraries(wubushow_app PRIVATE wubuoxml)

add_library(wuburead_app OBJECT apps/wuburead/main.c)
target_include_directories(wuburead_app PRIVATE src/wubuoxml src/wubuzip src/wubuxml)
target_link_libraries(wuburead_app PRIVATE wubuoxml)

add_library(wubuedit_app OBJECT apps/wubuedit/edit.c apps/wubuedit/docmodel.c)
target_include_directories(wubuedit_app PRIVATE src/wubuoxml src/wubuxml apps/wubuword apps/wubucell apps/wubushow)
target_link_libraries(wubuedit_app PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)

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
target_link_libraries(wubuedit PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)

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

add_executable(test_doc_cache tests/test_doc_cache.c)
target_link_libraries(test_doc_cache PRIVATE wubumodel)
add_test(NAME doc_cache COMMAND test_doc_cache)

add_executable(test_load_async tests/test_load_async.c)
target_link_libraries(test_load_async PRIVATE wubumodel)
add_test(NAME load_async COMMAND test_load_async)

add_executable(test_deflate_roundtrip tests/test_deflate_roundtrip.c)
target_link_libraries(test_deflate_roundtrip PRIVATE wubuzip)
add_test(NAME deflate_roundtrip COMMAND test_deflate_roundtrip)

add_executable(test_cold_start tests/test_cold_start.c)
target_link_libraries(test_cold_start PRIVATE wubumodel)
add_test(NAME cold_start COMMAND test_cold_start)

add_executable(test_model_io_roundtrip tests/test_model_io_roundtrip.c)
target_link_libraries(test_model_io_roundtrip PRIVATE wubumodel)
add_test(NAME model_io_roundtrip COMMAND test_model_io_roundtrip)

add_executable(test_wubuxml_parser tests/test_wubuxml_parser.c)
target_link_libraries(test_wubuxml_parser PRIVATE wubuoxml)
target_include_directories(test_wubuxml_parser PRIVATE src/wubuxml)
add_test(NAME wubuxml_parser COMMAND test_wubuxml_parser)

add_executable(test_reader tests/test_reader.c)
target_link_libraries(test_reader PRIVATE wubuoxml wubuword_app)
target_include_directories(test_reader PRIVATE src/wubuoxml src/wubuxml apps/wubuword)
add_test(NAME reader_roundtrip COMMAND test_reader)

add_executable(test_extract tests/test_extract.c)
target_link_libraries(test_extract PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)
target_include_directories(test_extract PRIVATE src/wubuoxml src/wubuxml apps/wubuword apps/wubucell apps/wubushow)
add_test(NAME extract COMMAND test_extract)

add_executable(test_edit tests/test_edit.c)
target_link_libraries(test_edit PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app wubuedit_app)
target_include_directories(test_edit PRIVATE src/wubuoxml src/wubuxml apps/wubuword apps/wubucell apps/wubushow apps/wubuedit)
add_test(NAME edit_roundtrip COMMAND test_edit)

add_executable(test_conformance tests/test_conformance.c)
target_link_libraries(test_conformance PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)
target_include_directories(test_conformance PRIVATE src/wubuoxml src/wubuxml apps/wubuword apps/wubucell apps/wubushow)
target_compile_definitions(test_conformance PRIVATE CONFORMANCE_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/validate.py")
add_test(NAME conformance COMMAND test_conformance)

add_executable(test_cell_read tests/test_cell_read.c)
target_link_libraries(test_cell_read PRIVATE wubucell_app wubuoxml wubuxml wubuformula)
target_include_directories(test_cell_read PRIVATE apps/wubucell src/wubuoxml src/wubuxml src/wubuformula src/wubuformula/functions)
add_test(NAME cell_read COMMAND test_cell_read)

add_executable(test_cell_cycle tests/test_cell_cycle.c)
target_link_libraries(test_cell_cycle PRIVATE wubucell_app wubuformula)
target_include_directories(test_cell_cycle PRIVATE apps/wubucell src/wubuformula src/wubuformula/functions)
add_test(NAME cell_cycle COMMAND test_cell_cycle)
