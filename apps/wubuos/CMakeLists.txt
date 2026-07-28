# wubuos: unified WuBuOffice GUI shell (Document / Spreadsheet / Slide / OCR /
# Editor). SDL2 window hosting every engine behind a WuView adapter. The
# Editor tab embeds the REAL WuBuPad editor core (Notepad++ parity) by compiling
# its headless sources directly (cross-repo include of ~/WuBuPad/src).
find_package(SDL2 REQUIRED)
find_package(Freetype REQUIRED)

set(WUBUPAD_SRC ${CMAKE_SOURCE_DIR}/../WuBuPad/src)

add_executable(wubuos
    main.c
    wuos_font.c
    wuos_file.c
    plugin.c
    toast.c
    palette.c
    view_doc.c
    view_cell.c
    view_slide.c
    view_ocr.c
    view_editor.c
    view_compare.c
    view_settings.c
    findbar.c
    autocomp.c)
target_include_directories(wubuos PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel
    ${CMAKE_SOURCE_DIR}/src/wuburender
    ${CMAKE_SOURCE_DIR}/src/wubuocr
    ${CMAKE_SOURCE_DIR}/src/wubufont
    ${CMAKE_SOURCE_DIR}/apps/wubucell
    ${CMAKE_SOURCE_DIR}/src/wubudoc
    ${CMAKE_SOURCE_DIR}/src/wubuautosave
    ${CMAKE_SOURCE_DIR}/src/wubuspell
    ${CMAKE_SOURCE_DIR}/src/wubuchart
    ${CMAKE_SOURCE_DIR}/src/wubudraw
    ${CMAKE_SOURCE_DIR}/src/wubumath
    ${CMAKE_SOURCE_DIR}/src/wubuepub
    ${CMAKE_SOURCE_DIR}/src/wubua11y
    ${CMAKE_SOURCE_DIR}/src/wubusvg
    ${CMAKE_SOURCE_DIR}/src/wubusettings
    ${CMAKE_SOURCE_DIR}/src/wubushape
    ${CMAKE_SOURCE_DIR}/src/wubulayout
    ${CMAKE_SOURCE_DIR}/src/wubuexp
    ${CMAKE_SOURCE_DIR}/src/wubutoc
    ${CMAKE_SOURCE_DIR}/src/wubuscript
    ${WUBUPAD_SRC})
# WuBuPad headless core sources (SDL-free) compiled in directly.
# lex.c is included for the editor's syntax highlighter. Its lex_free() name
# collides with wubuocr's lexicon.c lex_free(); we mangle only that one symbol
# via a compile definition so WuBuPad's repo is left untouched. complete.c is
# still excluded (depends on the lexer + not needed for the shell).
target_sources(wubuos PRIVATE
    ${WUBUPAD_SRC}/buffer.c
    ${WUBUPAD_SRC}/doc.c
    ${WUBUPAD_SRC}/search.c
    ${WUBUPAD_SRC}/encode.c
    ${WUBUPAD_SRC}/diff.c
    ${WUBUPAD_SRC}/docs.c
    ${WUBUPAD_SRC}/json.c
    ${WUBUPAD_SRC}/lex.c)
target_compile_definitions(wubuos PRIVATE lex_free=wubupad_lex_free doc_create=wubupad_doc_create doc_text=wubupad_doc_text)
target_link_libraries(wubuos PRIVATE
    wubumodel wuburender wubuocr wubufont wubucell_app wubudoc wubuautosave
    wubuspell wubuchart wubudraw wubumath wubuepub wubua11y wubusvg
    wubusettings wubushape wubulayout wubuexp wubutoc wubuscript
    ${SDL2_LIBRARIES} ${FREETYPE_LIBRARIES} m dl)
target_compile_options(wubuos PRIVATE -Wall -Wextra -Wpedantic)

# Headless smoke test: create + render every view (real engines, no GUI).
# Verifies each WuView adapter renders without crashing and returns a buffer.
add_executable(test_view test_view.c wuos_font.c wuos_file.c plugin.c toast.c palette.c view_doc.c view_cell.c
    view_slide.c view_ocr.c view_editor.c view_compare.c view_settings.c findbar.c autocomp.c)
target_include_directories(test_view PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel
    ${CMAKE_SOURCE_DIR}/src/wuburender
    ${CMAKE_SOURCE_DIR}/src/wubuocr
    ${CMAKE_SOURCE_DIR}/src/wubufont
    ${CMAKE_SOURCE_DIR}/apps/wubucell
    ${CMAKE_SOURCE_DIR}/src/wubudoc
    ${CMAKE_SOURCE_DIR}/src/wubuautosave
    ${CMAKE_SOURCE_DIR}/src/wubuspell
    ${CMAKE_SOURCE_DIR}/src/wubuchart
    ${CMAKE_SOURCE_DIR}/src/wubudraw
    ${CMAKE_SOURCE_DIR}/src/wubumath
    ${CMAKE_SOURCE_DIR}/src/wubuepub
    ${CMAKE_SOURCE_DIR}/src/wubua11y
    ${CMAKE_SOURCE_DIR}/src/wubusvg
    ${CMAKE_SOURCE_DIR}/src/wubusettings
    ${CMAKE_SOURCE_DIR}/src/wubushape
    ${CMAKE_SOURCE_DIR}/src/wubulayout
    ${CMAKE_SOURCE_DIR}/src/wubuexp
    ${CMAKE_SOURCE_DIR}/src/wubutoc
    ${CMAKE_SOURCE_DIR}/src/wubuscript
    ${WUBUPAD_SRC})
target_sources(test_view PRIVATE
    ${WUBUPAD_SRC}/buffer.c ${WUBUPAD_SRC}/doc.c ${WUBUPAD_SRC}/search.c
    ${WUBUPAD_SRC}/encode.c ${WUBUPAD_SRC}/diff.c ${WUBUPAD_SRC}/docs.c ${WUBUPAD_SRC}/json.c
    ${WUBUPAD_SRC}/lex.c)
target_compile_definitions(test_view PRIVATE lex_free=wubupad_lex_free doc_create=wubupad_doc_create doc_text=wubupad_doc_text)
target_link_libraries(test_view PRIVATE
    wubumodel wuburender wubuocr wubufont wubucell_app wubudoc wubuautosave
    wubuspell wubuchart wubudraw wubumath wubuepub wubua11y wubusvg
    wubusettings wubushape wubulayout wubuexp wubutoc wubuscript
    ${SDL2_LIBRARIES} ${FREETYPE_LIBRARIES} m dl)
target_compile_options(test_view PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME view COMMAND test_view)
set_tests_properties(view PROPERTIES LABELS "view;apps")

# Headless test for the shell's pure-logic UI modules (toast UI-33, palette UI-29).
add_executable(test_shell_ui test_shell_ui.c toast.c palette.c)
target_include_directories(test_shell_ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(test_shell_ui PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME shell_ui COMMAND test_shell_ui)
set_tests_properties(shell_ui PROPERTIES LABELS "view;apps")

# Reference plugin: builds to a loadable .so demonstrating the C ABI.
add_library(sample_plugin MODULE plugins/sample/sample_plugin.c)
set_target_properties(sample_plugin PROPERTIES
    PREFIX "" OUTPUT_NAME "sample_plugin" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins")
target_include_directories(sample_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(sample_plugin PRIVATE -fvisibility=hidden -Wall -Wextra)
# plugin exposes only wuos_plugin_init (visibility default via macro)

# Headless plugin ABI test: build a tiny loader-less driver that dlopens the
# sample .so and exercises load->init->exec end to end.
add_executable(test_plugin_abi test_plugin_abi.c plugin.c)
target_include_directories(test_plugin_abi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_plugin_abi PRIVATE dl)
target_compile_options(test_plugin_abi PRIVATE -Wall -Wextra)
# Negative-test plugin: reports a WRONG ABI version so the loader must reject it.
add_library(badabi_plugin MODULE plugins/badabi/badabi_plugin.c)
set_target_properties(badabi_plugin PROPERTIES
    PREFIX "" OUTPUT_NAME "badabi_plugin" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins")
target_include_directories(badabi_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(badabi_plugin PRIVATE -fvisibility=hidden -Wall -Wextra)

add_test(NAME plugin_abi COMMAND test_plugin_abi
    "${CMAKE_CURRENT_BINARY_DIR}/plugins/sample_plugin.so"
    "${CMAKE_CURRENT_BINARY_DIR}/plugins/badabi_plugin.so")
set_tests_properties(plugin_abi PROPERTIES LABELS "plugin;apps")

# Unit test for the extracted opaque AutoComp engine (no GUI/SDL).
add_executable(test_autocomp test_autocomp.c autocomp.c)
target_include_directories(test_autocomp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WUBUPAD_SRC})
target_sources(test_autocomp PRIVATE
    ${WUBUPAD_SRC}/buffer.c ${WUBUPAD_SRC}/doc.c ${WUBUPAD_SRC}/search.c
    ${WUBUPAD_SRC}/encode.c ${WUBUPAD_SRC}/diff.c ${WUBUPAD_SRC}/json.c ${WUBUPAD_SRC}/lex.c)
target_compile_definitions(test_autocomp PRIVATE lex_free=wubupad_lex_free doc_create=wubupad_doc_create doc_text=wubupad_doc_text)
target_compile_options(test_autocomp PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME autocomp COMMAND test_autocomp)
set_tests_properties(autocomp PROPERTIES LABELS "autocomp;apps")

# Unit test for the extracted opaque FindBar engine (no GUI/SDL).
add_executable(test_findbar test_findbar.c findbar.c)
target_include_directories(test_findbar PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WUBUPAD_SRC})
target_sources(test_findbar PRIVATE
    ${WUBUPAD_SRC}/buffer.c ${WUBUPAD_SRC}/doc.c ${WUBUPAD_SRC}/search.c
    ${WUBUPAD_SRC}/encode.c ${WUBUPAD_SRC}/diff.c ${WUBUPAD_SRC}/json.c ${WUBUPAD_SRC}/lex.c)
target_compile_definitions(test_findbar PRIVATE lex_free=wubupad_lex_free doc_create=wubupad_doc_create doc_text=wubupad_doc_text)
target_compile_options(test_findbar PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME findbar COMMAND test_findbar)
set_tests_properties(findbar PROPERTIES LABELS "findbar;apps")
