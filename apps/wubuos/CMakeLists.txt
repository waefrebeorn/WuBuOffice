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
    view_doc.c
    view_cell.c
    view_slide.c
    view_ocr.c
    view_editor.c)
target_include_directories(wubuos PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel
    ${CMAKE_SOURCE_DIR}/src/wuburender
    ${CMAKE_SOURCE_DIR}/src/wubuocr
    ${CMAKE_SOURCE_DIR}/src/wubufont
    ${CMAKE_SOURCE_DIR}/apps/wubucell
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_SOURCE_DIR}/src/wubuxml
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
target_compile_definitions(wubuos PRIVATE lex_free=wubupad_lex_free)
target_link_libraries(wubuos PRIVATE
    wubumodel wuburender wubuocr wubufont wubucell_app ${SDL2_LIBRARIES} ${FREETYPE_LIBRARIES} m)
target_compile_options(wubuos PRIVATE -Wall -Wextra -Wpedantic)

# Headless smoke test: create + render every view (real engines, no GUI).
# Verifies each WuView adapter renders without crashing and returns a buffer.
add_executable(test_view test_view.c wuos_font.c wuos_file.c view_doc.c view_cell.c
    view_slide.c view_ocr.c view_editor.c)
target_include_directories(test_view PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/wubumodel
    ${CMAKE_SOURCE_DIR}/src/wuburender
    ${CMAKE_SOURCE_DIR}/src/wubuocr
    ${CMAKE_SOURCE_DIR}/src/wubufont
    ${CMAKE_SOURCE_DIR}/apps/wubucell
    ${CMAKE_SOURCE_DIR}/src/wubuoxml
    ${CMAKE_SOURCE_DIR}/src/wubuxml
    ${WUBUPAD_SRC})
target_sources(test_view PRIVATE
    ${WUBUPAD_SRC}/buffer.c ${WUBUPAD_SRC}/doc.c ${WUBUPAD_SRC}/search.c
    ${WUBUPAD_SRC}/encode.c ${WUBUPAD_SRC}/diff.c ${WUBUPAD_SRC}/docs.c ${WUBUPAD_SRC}/json.c
    ${WUBUPAD_SRC}/lex.c)
target_compile_definitions(test_view PRIVATE lex_free=wubupad_lex_free)
target_link_libraries(test_view PRIVATE
    wubumodel wuburender wubuocr wubufont wubucell_app ${SDL2_LIBRARIES} ${FREETYPE_LIBRARIES} m)
target_compile_options(test_view PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME view COMMAND test_view)
set_tests_properties(view PROPERTIES LABELS "view")
